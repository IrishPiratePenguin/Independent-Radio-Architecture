// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// The YSF engine (core/ysf/ysf_engine.c) and the air interface session under
// it: two engines on two made-up providers whose transmit queues feed each
// other's receivers a chunk at a time. A call from one is heard by the other
// with its fields and its voice in order, in V/D-1 and in DN; an encoder that
// stalls is covered with silence and never underruns the air; late entry; the
// DG-ID squelch; an abort; narrow deviation. Voice frames are valid AMBE
// frames (ambeFecEncode of random parameters), so the DN path converts real
// channel frames, not patterns.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ysf/ysf_engine.h"
#include "ysf/ysf_voice.h"
#include "air/air_syms.h"
#include "dmr/ambe/ambeFec.h"

static int failures, checks;
static FILE *capture;
static FILE *speech;
static bool speech_error;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t rng_state = 0x59534621u;
static uint32_t rng(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

// ── Two providers, each other's far end. A chunk is 30 ms of symbols at the session's rate ──
#define CHUNK_MAX	144
#define DEPTH		4
typedef struct {
	air_event_cb_t cb;
	void *ctx;
	bool open, on, begun_ev;
	uint16_t chunk;
	air_tx_chunk_t q[DEPTH];
	uint8_t qn;
	uint32_t underruns, sent_syms, now_us;
	bool keyed;
	uint32_t keyed_violations;
} prov_t;
static prov_t prov[2];

static void ev(prov_t *p, uint8_t kind, const air_chunk_t *c)
{
	air_event_t e = { .kind = kind, .t_us = p->now_us, .chunk = c };

	if (p->cb)
		p->cb(&e, p->ctx);
}

static void deliver(prov_t *to, const uint8_t *syms, uint16_t n)
{
	int8_t soft[CHUNK_MAX];
	air_chunk_t c = { .t_us = to->now_us, .nsyms = n, .sync_at = -1, .hard = syms, .soft = soft };

	for (uint16_t i = 0; i < n; i++)
		soft[i] = (int8_t)(32 * air_sym_level(air_sym_get(syms, i)));
	if (to->open && !to->on)
		ev(to, AIR_EV_RX, &c);
}

static void step(uint8_t i)
{
	prov_t *p = &prov[i], *far = &prov[i ^ 1];

	p->now_us += 30000;
	if (!p->on) {
		if (!far->on) {
			uint8_t noise[CHUNK_MAX / 4];

			for (uint8_t k = 0; k < sizeof(noise); k++)
				noise[k] = (uint8_t)rng();
			deliver(p, noise, p->chunk);
		}
		return;
	}
	if (!p->keyed)
		p->keyed_violations++;
	if (!p->begun_ev) {
		p->begun_ev = true;
		ev(p, AIR_EV_TX_STARTED, NULL);
	}
	if (!p->qn) {
		uint8_t idle[CHUNK_MAX / 4];

		memset(idle, 0xD4, sizeof(idle));	// the YSF idle dibit pattern
		p->underruns++;
		deliver(far, idle, p->chunk);
		ev(p, AIR_EV_TX_UNDERRUN, NULL);
		return;
	}
	air_tx_chunk_t c = p->q[0];

	memmove(p->q, p->q + 1, (size_t)(--p->qn) * sizeof(c));
	p->sent_syms += c.nsyms;
	if (capture && i == 0)
		for (uint16_t j = 0; j < c.nsyms; j++)
			fputc(air_sym_get(c.syms, j), capture);
	deliver(far, c.syms, c.nsyms);
	if (c.flags & AIR_TX_LAST) {
		p->on = false;
		ev(p, AIR_EV_TX_DONE, NULL);
	} else {
		ev(p, AIR_EV_TX_SPACE, NULL);
	}
}

// The board's answer: YSF (4800 symbols/s, one sync), nothing else
static uint32_t p_fit(const air_profile_t *pr)
{
	return pr->sym_rate == 4800 && pr->nsyncs == 1 ? AIR_FIT_RX | AIR_FIT_TX | AIR_FIT_SOFT : 0;
}
static bool p_open(uint8_t i, const air_profile_t *pr, air_event_cb_t cb, void *ctx)
{
	memset(&prov[i], 0, sizeof(prov[i]));
	prov[i].cb = cb;
	prov[i].ctx = ctx;
	prov[i].open = true;
	prov[i].chunk = (uint16_t)(pr->sym_rate * 3 / 100);
	return true;
}
static void p_close(uint8_t i) { prov[i].open = prov[i].on = false; prov[i].cb = NULL; }
static bool p_begin(uint8_t i) { if (!prov[i].qn) return false; prov[i].on = true; prov[i].begun_ev = false; return true; }
static bool p_queue(uint8_t i, const air_tx_chunk_t *c)
{
	if (prov[i].qn == DEPTH)
		return false;
	prov[i].q[prov[i].qn++] = *c;
	return true;
}
static void p_end(uint8_t i) { if (prov[i].qn) prov[i].q[prov[i].qn - 1].flags |= AIR_TX_LAST; }
static void p_abort(uint8_t i) { prov[i].qn = 0; prov[i].on = false; ev(&prov[i], AIR_EV_TX_DONE, NULL); }

#define PROVIDER(i) \
	static bool open##i(const air_profile_t *pr, air_event_cb_t cb, void *ctx) { return p_open(i, pr, cb, ctx); } \
	static void close##i(void) { p_close(i); } \
	static void restart##i(void) { } \
	static bool begin##i(void) { return p_begin(i); } \
	static bool queue##i(const air_tx_chunk_t *c) { return p_queue(i, c); } \
	static uint8_t space##i(void) { return (uint8_t)(DEPTH - prov[i].qn); } \
	static void end##i(void) { p_end(i); } \
	static void abort##i(void) { p_abort(i); } \
	static uint32_t now##i(void) { return prov[i].now_us; } \
	static hal_air_t air##i = { p_fit, open##i, close##i, restart##i, begin##i, queue##i, space##i, end##i, abort##i, \
				    now##i, NULL, NULL, NULL, NULL, 72, DEPTH, 60, 100 };
PROVIDER(0)
PROVIDER(1)

// ── The two radios ──
// The DMR AMBE silence frame (core/dmr/call/dmr_call.c): a valid frame, as the voice path needs
static const uint8_t silence[9] = { 0xB9, 0xE8, 0x81, 0x52, 0x61, 0x73, 0x00, 0x2A, 0x6B };
#define VMAX	240
typedef struct {
	ysf_engine_t e;
	uint8_t starts, updates, ends;
	ysf_call_t call;
	uint8_t voice[VMAX][9];
	uint16_t nvoice;
	uint8_t enc[VMAX][9];
	uint16_t enc_made, enc_taken;
	bool stuck;		// the encoder never stops: h_tx_voice keeps returning blocks after the key
	uint8_t data[16][20];
	uint8_t data_len[16];
	uint16_t ndata;
} radio_t;
static radio_t r[2];

static void h_start(const ysf_call_t *c, void *ctx) { radio_t *x = ctx; x->starts++; x->call = *c; }
static void h_update(const ysf_call_t *c, void *ctx) { radio_t *x = ctx; x->updates++; x->call = *c; }
static void h_end(void *ctx) { radio_t *x = ctx; x->ends++; }
static void h_voice(const uint8_t vch[9], void *ctx)
{
	radio_t *x = ctx;

	if (x->nvoice < VMAX)
		memcpy(x->voice[x->nvoice++], vch, 9);
}
static void h_data(void *ctx, unsigned index, const uint8_t *data, unsigned len)
{
	radio_t *x = ctx;

	if (index < 16 && x->ndata < 16) {
		memcpy(x->data[x->ndata], data, len);
		x->data_len[x->ndata] = (uint8_t)len;
		x->ndata++;
	}
}
static bool h_tx_voice(uint8_t vch[9], void *ctx)
{
	radio_t *x = ctx;

	if (x->stuck) {
		memcpy(vch, silence, 9);	// the encoder never stops: blocks keep coming after the key
		return true;
	}
	if (x->enc_taken == x->enc_made)
		return false;
	memcpy(vch, x->enc[x->enc_taken++], 9);
	return true;
}
static void h_tx_data(void *ctx, unsigned index, uint8_t out[20])
{
	(void)ctx;
	for (unsigned k = 0; k < 20; k++)
		out[k] = (uint8_t)index;
}
static const ysf_engine_hooks_t hooks = { .rx_start = h_start, .rx_update = h_update, .rx_voice = h_voice,
					   .rx_end = h_end, .rx_data = h_data, .tx_voice = h_tx_voice,
					   .tx_data = h_tx_data };

static void encode(radio_t *x, uint8_t n)
{
	while (n-- && x->enc_made < VMAX) {
		uint8_t bits[AMBE_PARAM_BITS];

		if (speech) {
			uint8_t packed[(AMBE_PARAM_BITS + 7) / 8];
			size_t nread = fread(packed, 1, sizeof packed, speech);

			if (nread != sizeof packed) {
				speech_error |= nread != 0 || ferror(speech);
				return;
			}
			for (uint8_t k = 0; k < AMBE_PARAM_BITS; k++)
				bits[k] = (uint8_t)((packed[k / 8] >> (7 - k % 8)) & 1u);
		} else {
			for (uint8_t k = 0; k < AMBE_PARAM_BITS; k++)
				bits[k] = (uint8_t)(rng() & 1u);
		}
		ambeFecEncode(bits, x->enc[x->enc_made]);
		x->enc_made++;
	}
}

static void begin(uint8_t i, const ysf_engine_cfg_t *cfg, const ysf_engine_hooks_t *h)
{
	hal_air_t *air = i ? &air1 : &air0;

	memset(&r[i], 0, sizeof(r[i]));
	air->tx_chunk_syms = (uint16_t)(ysf_engine_mode(cfg->narrow)->profile.sym_rate * 3 / 100);
	CHECK(ysf_engine_begin(&r[i].e, air, cfg, h, &r[i]), "radio %u begins", i);
}

static void steps(uint16_t n)
{
	while (n--) {
		step(0);
		step(1);
	}
}

// Radio 0 transmits: `n` chunk periods with the key down (the encoder makes 3 blocks per 2 periods, none during
// stall_from..stall_to), then lets go and runs until the air is quiet. Returns the periods it took
static uint16_t over(uint16_t n_key, uint16_t stall_from, uint16_t stall_to)
{
	uint16_t n = 0;

	CHECK(ysf_engine_tx_prepare(&r[0].e), "the header is queued");
	CHECK(ysf_engine_tx_state(&r[0].e) == AIR_TX_PRIMING && !prov[0].on, "primed, not sending");
	prov[0].keyed = true;
	CHECK(ysf_engine_tx_go(&r[0].e), "go");
	for (; n < 600; n++) {
		if (n == n_key)
			ysf_engine_tx_stop(&r[0].e);
		if (n < n_key && !(n >= stall_from && n < stall_to))
			encode(&r[0], (uint8_t)(1 + (n & 1)));
		steps(1);
		if (ysf_engine_tx_state(&r[0].e) == AIR_TX_DONE) {
			prov[0].keyed = false;
			ysf_engine_tx_clear(&r[0].e);
			break;
		}
	}
	steps(20);
	return n;
}

// The blocks heard against the blocks made: the made ones in order (silence between them is filler), from the
// first heard. Returns the number out of order or wrong
static uint16_t check_order(const radio_t *rx, const radio_t *tx, uint16_t *first)
{
	uint16_t bad = 0, k = 0, i = 0;

	while (i < rx->nvoice && !memcmp(rx->voice[i], silence, 9))
		i++;
	while (k < tx->enc_made && i < rx->nvoice && memcmp(rx->voice[i], tx->enc[k], 9))
		k++;	// a late entry: the first ones are missing
	*first = k;
	for (; i < rx->nvoice; i++) {
		if (!memcmp(rx->voice[i], silence, 9))
			continue;
		if (k >= tx->enc_made || memcmp(rx->voice[i], tx->enc[k++], 9))
			bad++;
	}
	return bad;
}

static ysf_engine_cfg_t base_cfg(uint8_t dt, bool narrow)
{
	ysf_engine_cfg_t cfg = { .dt = dt, .narrow = narrow, .rx_dgid = 0, .tx_dgid = 7,
				 .cm = YSF_CM_GROUP, .mr = YSF_MR_DIRECT, .silence = silence };

	memcpy(cfg.dest, "CQ        ", 10);
	memcpy(cfg.src, "XE1ARS    ", 10);
	memcpy(cfg.down, "DOWN      ", 10);
	memcpy(cfg.up, "UP        ", 10);
	memcpy(cfg.rem1, "REM1      ", 10);
	memcpy(cfg.rem2, "REM2      ", 10);
	memcpy(cfg.rem3, "REM3      ", 10);
	memcpy(cfg.rem4, "REM4      ", 10);
	return cfg;
}

static void testCall(uint8_t dt, bool narrow)
{
	ysf_engine_cfg_t cfg = base_cfg(dt, narrow);
	ysf_engine_view_t v;
	uint16_t n, first, bad;
	const char *name = dt == YSF_DT_VD_MODE1 ? "V/D-1" : "DN";

	CHECK(ysf_engine_fit(&air0, narrow) == (AIR_FIT_RX | AIR_FIT_TX | AIR_FIT_SOFT) &&
	      ysf_engine_fit(NULL, false) == 0, "fit");
	begin(0, &cfg, &hooks);
	begin(1, &cfg, &hooks);
	steps(10);
	CHECK(r[0].starts == 0 && r[1].starts == 0, "%s: noise starts no call", name);

	n = over(100, 0, 0);	// 3 s
	// The end: the frame in hand finished and a terminator (100 ms), plus the queue's lead (60 ms + one chunk).
	// Filler: a block waits for its voice only while the queue holds more than its lead, so the first ones and
	// the last (a frame finished after the key) may be silence
	CHECK(n > 100 && n < 112, "%s: the over ended %u periods after 100", name, n);
	CHECK(prov[0].underruns == 0 && prov[0].keyed_violations == 0 && r[0].e.session.stats.tx_underruns == 0,
	      "%s: underruns %u, keyed outside permission %u", name, prov[0].underruns, prov[0].keyed_violations);
	ysf_engine_view(&r[0].e, &v);
	CHECK(v.tx == AIR_TX_IDLE && v.tx_calls == 1 && v.tx_filler <= 40, "%s: sender's view: tx %u calls %u filler %u",
	      name, v.tx, v.tx_calls, v.tx_filler);
	CHECK(r[1].starts == 1 && r[1].ends == 1 && !r[1].call.late && !strcmp(r[1].call.dest, "CQ") &&
	      !strcmp(r[1].call.src, "XE1ARS") && !strcmp(r[1].call.down, "DOWN") &&
	      !strcmp(r[1].call.up, "UP") && !strcmp(r[1].call.rem1, "REM1") &&
	      !strcmp(r[1].call.rem2, "REM2") && r[1].call.fich.dt == dt && r[1].call.fich.sc == 7 &&
	      r[1].call.fich.dev == (narrow ? 1 : 0),
	      "%s: heard: starts %u ends %u dest %s src %s", name, r[1].starts, r[1].ends, r[1].call.dest,
	      r[1].call.src);
	bad = check_order(&r[1], &r[0], &first);
	CHECK(bad == 0 && first == 0 && r[0].enc_taken == r[0].enc_made && r[1].nvoice % 5 == 0 &&
	      r[1].nvoice == r[0].enc_made + v.tx_filler,
	      "%s: blocks made %u heard %u, %u wrong, filler %u", name, r[0].enc_made, r[1].nvoice, bad,
	      v.tx_filler);
	ysf_engine_view(&r[1].e, &v);
	CHECK(!v.rx_active && v.rx_calls == 1 && !strcmp(v.rx.dest, "CQ") && v.rx_frames >= 10,
	      "%s: receiver's view after the call", name);
	CHECK(r[0].starts == 0, "%s: the sender did not hear itself", name);
	CHECK(r[1].ndata >= 2, "%s: data units heard %u", name, r[1].ndata);
	printf("  %s%s: %u voice blocks sent, %u heard, over ended %u periods after the key\n", name,
	       narrow ? " narrow" : "", r[0].enc_made, r[1].nvoice, n - 100);

	// The encoder stalls for 300 ms mid-over: silence fills in, the air never runs dry
	r[1].starts = r[1].ends = 0;
	r[1].nvoice = 0;
	r[1].ndata = 0;
	r[0].enc_made = r[0].enc_taken = 0;
	over(60, 20, 30);
	ysf_engine_view(&r[0].e, &v);
	CHECK(prov[0].underruns == 0 && v.tx_filler >= 10 && v.tx_filler <= 40, "%s: stall: underruns %u filler %u",
	      name, prov[0].underruns, v.tx_filler);
	CHECK(r[1].starts == 1 && r[1].ends == 1 && check_order(&r[1], &r[0], &first) == 0,
	      "%s: stall: starts %u ends %u", name, r[1].starts, r[1].ends);

	// The encoder never stops after the key: the terminator is forced past the bound, so the over still ends
	// (without it the transmitter would stay on until the radio is switched off)
	r[1].starts = r[1].ends = 0;
	r[1].nvoice = 0;
	r[1].ndata = 0;
	r[0].enc_made = r[0].enc_taken = 0;
	r[0].stuck = false;
	CHECK(ysf_engine_tx_prepare(&r[0].e), "prime");
	prov[0].keyed = true;
	CHECK(ysf_engine_tx_go(&r[0].e), "go");
	for (n = 0; n < 600; n++) {
		if (n == 100) {
			ysf_engine_tx_stop(&r[0].e);
			r[0].stuck = true;	// the voice source keeps feeding after the key
		}
		if (n < 100)
			encode(&r[0], (uint8_t)(1 + (n & 1)));
		steps(1);
		if (ysf_engine_tx_state(&r[0].e) == AIR_TX_DONE) {
			prov[0].keyed = false;
			ysf_engine_tx_clear(&r[0].e);
			break;
		}
	}
	steps(20);
	CHECK(n > 100 && n < 170 && prov[0].underruns == 0,
	      "%s: stuck encoder: the over ended %u periods after 100, underruns %u", name, n,
	      prov[0].underruns);
	CHECK(r[1].starts == 1 && r[1].ends == 1 && check_order(&r[1], &r[0], &first) == 0,
	      "%s: stuck encoder: starts %u ends %u", name, r[1].starts, r[1].ends);
	r[0].stuck = false;

	// Late entry: radio 1 opens its session 1 s into the over. Its voice starts at a frame's end, in order (the
	// queue's lead of about 90 ms puts the frame on air at 990 ms a few blocks back)
	ysf_engine_end(&r[1].e);
	r[0].enc_made = r[0].enc_taken = 0;
	CHECK(ysf_engine_tx_prepare(&r[0].e), "prime");
	prov[0].keyed = true;
	CHECK(ysf_engine_tx_go(&r[0].e), "go");
	for (n = 0; n < 300; n++) {
		if (n == 33)
			begin(1, &cfg, &hooks);
		if (n == 100)
			ysf_engine_tx_stop(&r[0].e);
		if (n < 100)
			encode(&r[0], (uint8_t)(1 + (n & 1)));
		steps(1);
		if (ysf_engine_tx_state(&r[0].e) == AIR_TX_DONE) {
			prov[0].keyed = false;
			ysf_engine_tx_clear(&r[0].e);
			break;
		}
	}
	steps(20);
	bad = check_order(&r[1], &r[0], &first);
	printf("  %s late entry at 990 ms: first block heard %u of %u (%u ms of voice), %u wrong\n", name, first,
	       r[0].enc_made, first * 20, bad);
	CHECK(r[1].starts == 1 && r[1].call.late && r[1].ends == 1 && bad == 0 && first >= 40 && first <= 70,
	      "%s: late entry: starts %u late %u first %u", name, r[1].starts, r[1].call.late, first);
	ysf_engine_end(&r[0].e);
	ysf_engine_end(&r[1].e);
}

static void testSquelch(void)
{
	ysf_engine_cfg_t cfg = base_cfg(YSF_DT_VD_MODE1, false);
	ysf_engine_view_t v;

	// A DG-ID the squelch does not take: the call is not even seen (its
	// frames are dropped before any call starts)
	begin(0, &cfg, &hooks);
	cfg.rx_dgid = 8;
	begin(1, &cfg, &hooks);
	CHECK(ysf_engine_tx_prepare(&r[0].e), "prime");
	prov[0].keyed = true;
	CHECK(ysf_engine_tx_go(&r[0].e), "go");
	for (uint8_t k = 0; k < 20; k++) {
		encode(&r[0], (uint8_t)(1 + (k & 1)));
		steps(1);
	}
	ysf_engine_view(&r[1].e, &v);
	CHECK(!v.rx_active && !v.rx_played && v.rx_rejects > 0 && r[1].starts == 0 && r[1].nvoice == 0,
	      "another DG-ID: active %d played %d rejects %u heard %u", v.rx_active, v.rx_played, v.rx_rejects,
	      r[1].nvoice);
	// The operator switches to that DG-ID mid-call: the next frames open the squelch (late entry)
	cfg.rx_dgid = 7;
	ysf_engine_set(&r[1].e, &cfg);
	for (uint8_t k = 0; k < 20; k++) {
		encode(&r[0], (uint8_t)(1 + (k & 1)));
		steps(1);
	}
	CHECK(r[1].starts == 1 && r[1].call.late && r[1].nvoice >= 5, "our DG-ID now: starts %u heard %u",
	      r[1].starts, r[1].nvoice);
	// Abort: off the air at once, the far end sees the signal go
	ysf_engine_tx_abort(&r[0].e);
	CHECK(ysf_engine_tx_state(&r[0].e) == AIR_TX_DONE && !prov[0].on, "aborted");
	prov[0].keyed = false;
	ysf_engine_tx_clear(&r[0].e);
	steps(30);
	ysf_engine_view(&r[1].e, &v);
	CHECK(r[1].ends == 1 && !v.rx_active, "the far end's call ended after the abort: %u", r[1].ends);

	// A closed engine refuses
	ysf_engine_end(&r[0].e);
	CHECK(!ysf_engine_tx_prepare(&r[0].e), "closed: no transmit");
	ysf_engine_end(&r[1].e);
}

static void send_frame(const uint8_t frame[YSF_FRAME_BYTES])
{
	for (unsigned i = 0; i < YSF_FRAME_DIBITS; i += CHUNK_MAX) {
		unsigned remaining = YSF_FRAME_DIBITS - i;

		deliver(&prov[1], frame + i / 4, remaining < CHUNK_MAX ? remaining : CHUNK_MAX);
	}
}

static void testBadData(uint8_t dt)
{
	ysf_engine_cfg_t cfg = base_cfg(dt, false);
	ysf_fich fich = { .fi = YSF_FI_HEADER, .sc = 7, .dt = dt };
	const uint8_t csd1[20] = "DEST      SOURCE    ";
	const uint8_t csd2[20] = "DOWN      UP        ";
	uint8_t frame[YSF_FRAME_BYTES];

	begin(1, &cfg, &hooks);
	ysf_frame_build(&fich, csd1, csd2, frame);
	send_frame(frame);
	CHECK(!strcmp(r[1].e.call.src, "SOURCE"), "call header gives the source");

	fich.fi = YSF_FI_COMMUNICATION;
	fich.ft = 7;
	fich.fn = dt == YSF_DT_VD_MODE2 ? 6 : 3;
	ysf_frame_build(&fich, csd1, NULL, frame);
	for (unsigned b = 0; b < YSF_BLOCKS; b++) {
		unsigned dch_bytes = dt == YSF_DT_VD_MODE2 ? 5 : 9;
		uint8_t *payload = frame + YSF_SYNC_BYTES + YSF_FICH_BYTES + 18 * b;

		memset(payload, 0, dch_bytes);
		if (dt == YSF_DT_VD_MODE2)
			ysf_voice_dn_from_frame(silence, payload + dch_bytes);
		else
			memcpy(payload + dch_bytes, silence, sizeof silence);
	}
	send_frame(frame);
	CHECK(r[1].e.rx.dch_fail == 1 && r[1].ndata == 0, "bad data is counted and never delivered");
	CHECK(r[1].nvoice == YSF_BLOCKS && !strcmp(r[1].e.call.src, "SOURCE"),
	      "bad data leaves all five voice blocks and the callsign intact");
	for (unsigned b = 0; b < YSF_BLOCKS && b < r[1].nvoice; b++)
		CHECK(!memcmp(r[1].voice[b], silence, sizeof silence), "voice block %u survives unchanged", b);
	ysf_engine_end(&r[1].e);
}

static const uint8_t vw_silence[FEC_IMBE88_BYTES] = { 0xD8 };
static uint16_t vw_made, vw_taken, vw_heard;
static uint8_t vw_rx[VMAX][FEC_IMBE88_BYTES];

static void vw_params(uint16_t sample, uint8_t imbe[FEC_IMBE88_BYTES])
{
	for (unsigned i = 0; i < FEC_IMBE88_BYTES; i++)
		imbe[i] = (uint8_t)(3u * sample + i * 17u);
}

static bool vw_encode(uint8_t imbe[FEC_IMBE88_BYTES], void *ctx)
{
	(void)ctx;
	if (vw_taken == vw_made)
		return false;
	vw_params(vw_taken++, imbe);
	return true;
}

static void vw_play(const uint8_t imbe[FEC_IMBE88_BYTES], uint8_t errs, void *ctx)
{
	(void)ctx;
	CHECK(errs == 0, "VW channel FEC succeeds");
	if (vw_heard < VMAX)
		memcpy(vw_rx[vw_heard++], imbe, FEC_IMBE88_BYTES);
}

static void testVW(bool narrow)
{
	ysf_engine_cfg_t cfg = base_cfg(YSF_DT_VOICE_FR, narrow);
	ysf_engine_hooks_t vw = hooks;
	uint16_t n;
	uint16_t heard = 0;

	vw.rx_vw = vw_play;
	vw.tx_vw = vw_encode;
	cfg.silence_vw = vw_silence;
	vw_made = vw_taken = vw_heard = 0;
	begin(0, &cfg, &vw);
	begin(1, &cfg, &vw);
	CHECK(ysf_engine_tx_prepare(&r[0].e), "VW primes");
	prov[0].keyed = true;
	CHECK(ysf_engine_tx_go(&r[0].e), "VW keys");
	for (n = 0; n < 115; n++) {
		if (n < 80)
			vw_made += (uint16_t)(1 + (n & 1));
		if (n == 80)
			ysf_engine_tx_stop(&r[0].e);
		steps(1);
		if (ysf_engine_tx_state(&r[0].e) == AIR_TX_DONE)
			break;
	}
	CHECK(n < 115 && prov[0].underruns == 0 && r[1].ends == 1, "VW tail terminates cleanly");
	for (unsigned i = 0; i < vw_heard; i++) {
		uint8_t expected[FEC_IMBE88_BYTES];

		if (!memcmp(vw_rx[i], vw_silence, sizeof vw_silence))
			continue;
		vw_params(heard++, expected);
		CHECK(!memcmp(vw_rx[i], expected, sizeof expected), "VW keeps block order at %u", i);
	}
	CHECK(vw_taken == vw_made && heard == vw_made && vw_heard % 5 == 2,
	      "VW all %u blocks heard (%u with filler), exactly one two-block sub-header", vw_made, vw_heard);
	CHECK(r[1].starts == 1 && r[1].nvoice == 0 && r[1].ndata == 0 &&
	      !strcmp(r[1].call.rem1, "REM1") && !strcmp(r[1].call.rem2, "REM2"),
	      "VW preserves CSD3 and uses only the full-rate voice hooks");
	ysf_engine_end(&r[0].e);
	ysf_engine_end(&r[1].e);
}

// Replay MSB-first packed AMBE parameters through the firmware's TX engine.
static int capture_call(const char *mode, const char *output)
{
	uint8_t dt;

	if (!strcmp(mode, "vd1"))
		dt = YSF_DT_VD_MODE1;
	else if (!strcmp(mode, "dn"))
		dt = YSF_DT_VD_MODE2;
	else
		return 1;
	capture = fopen(output, "wb");
	if (!capture) {
		perror(output);
		return 1;
	}
	speech = stdin;
	// Give the file decoder some symbols to establish slicing levels before sync.
	for (unsigned i = 0; i < 500; i++)
		fputc(rng() & 3, capture);
	ysf_engine_cfg_t cfg = base_cfg(dt, false);
	begin(0, &cfg, &hooks);
	uint16_t periods = over(160, 0, 0);
	int written = fclose(capture);
	ysf_engine_end(&r[0].e);
	return failures || speech_error || written || periods == 600 || prov[0].underruns ||
		r[0].enc_made != r[0].enc_taken;
}

int main(int argc, char **argv)
{
	if (argc == 4 && !strcmp(argv[1], "--capture"))
		return capture_call(argv[2], argv[3]);
	if (argc != 1) {
		fprintf(stderr, "Usage: %s [--capture vd1|dn output.bin < speech.ambe]\n", argv[0]);
		return 1;
	}
	testCall(YSF_DT_VD_MODE1, false);
	testCall(YSF_DT_VD_MODE2, false);
	testCall(YSF_DT_VD_MODE1, true);
	testSquelch();
	testBadData(YSF_DT_VD_MODE1);
	testBadData(YSF_DT_VD_MODE2);
	testVW(false);
	testVW(true);
	printf("ysfEngineTest: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
