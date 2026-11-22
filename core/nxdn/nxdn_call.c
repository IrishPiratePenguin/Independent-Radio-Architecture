// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Copyright (c) 2026 lyra XE1POC (changes)
// A conventional NXDN voice call (nxdn.h): on receive, the frames of nxdn_rx.c become a call with its fields and its
// voice frames in order; on transmit, the frames are built and released symbol by symbol. Section numbers are NXDN
// TS 1-B's (Basic Operation) where the text says so, else TS 1-A's.

#include "nxdn.h"
#if CONFIG_AIR_AES
#include "nxdn_aes.h"
#endif

#include <string.h>

static uint8_t bit_get(const uint8_t *p, uint16_t i)
{
	return (uint8_t)((p[i >> 3] >> (7 - (i & 7))) & 1);
}

static void bit_put(uint8_t *p, uint16_t i, uint8_t b)
{
	uint8_t m = (uint8_t)(0x80u >> (i & 7));

	p[i >> 3] = (uint8_t)(b ? (p[i >> 3] | m) : (p[i >> 3] & ~m));
}

// ── Receive ──
static void call_start(nxdn_call_rx_t *c, const nxdn_rx_frame_t *f, bool late)
{
	memset(&c->call, 0, sizeof c->call);
	c->call.late = late;
	c->call.ran = f->sacch_errs >= 0 ? NXDN_SR_RAN(f->sacch[0]) : c->rx.ran;
	c->call.rssi_dbm = f->rssi_dbm;
	c->active = true;
#if CONFIG_AIR_AES
	c->anchored = c->next_iv_valid = c->sync_next = false;
	c->vch_n = 0;
#endif
	c->stats.calls++;
	if (c->sink->start)
		c->sink->start(&c->call, c->ctx);
}

static void call_end(nxdn_call_rx_t *c)
{
	c->sf_parts = 0;
	c->sf_next = -1;
	if (!c->active)
		return;
	c->active = false;
	if (c->sink->end)
		c->sink->end(c->ctx);
}

// Voice that can be played: the call's VCALL is in (a late entry waits for a whole superframe of SACCH, TS 1-B
// 4.1.6), clear (TS 1-B 4.1.7: encrypted voice is noise without the key) and half rate (the vocoder's)
static bool playable(const nxdn_call_t *call)
{
	uint8_t mode = call->vcall.voice_opt & 7;

	return call->have_vcall && ((call->vcall.cipher == NXDN_CIPHER_NONE && !call->iv) || call->decrypt) &&
	       (mode == NXDN_VOICE_4800_EHR || mode == NXDN_VOICE_9600_EHR);
}

#if CONFIG_AIR_AES
void nxdn_call_rx_set_key(nxdn_call_rx_t *c, const uint8_t *key, uint8_t kid)
{
	c->aes_key = key;
	c->aes_kid = kid;
}

// Whether the call's voice can be decrypted: AES-256 with the key id of the key we hold, and the IV of the header
static bool aes_ready(const nxdn_call_rx_t *c)
{
	return c->aes_key != NULL && c->call.have_vcall && c->call.vcall.cipher == NXDN_CIPHER_AES && c->call.iv &&
	       c->anchored && c->call.vcall.key_id == c->aes_kid;
}

// The call's fields changed: decide again whether it is decrypted. True: it changed
static bool aes_judge(nxdn_call_rx_t *c)
{
	bool ready = aes_ready(c);

	if (ready == c->call.decrypt)
		return false;
	c->call.decrypt = ready;
	return true;
}
#endif

// A layer 3 message from a FACCH1 or a superframe's SACCH. head: from a call's first frame (non-superframe)
static void on_msg(nxdn_call_rx_t *c, const nxdn_rx_frame_t *f, const uint8_t *msg, bool head, bool sacch)
{
	nxdn_vcall_t v;

	switch (nxdn_msg_type(msg)) {
	case NXDN_MSG_VCALL:
		if (!nxdn_vcall_parse(msg, &v))
			return;
		// A header after voice: a new call whose start the old one's release was lost before
		if (c->active && head && c->call.have_vcall && memcmp(&v, &c->call.vcall, sizeof v))
			call_end(c);
		if (!c->active)
			call_start(c, f, !head);
#if CONFIG_AIR_AES
		if (head) {
			c->vch_n = 0;		// the header: the superframe frames count from here, as the sender's do
			c->anchored = false;
			c->next_iv_valid = c->sync_next = false;
		}
#endif
		if (!c->call.have_vcall || memcmp(&v, &c->call.vcall, sizeof v)) {
			c->call.vcall = v;
			c->call.have_vcall = true;
#if CONFIG_AIR_AES
			aes_judge(c);
#endif
			if (c->sink->update)
				c->sink->update(&c->call, c->ctx);
		}
		break;
	case NXDN_MSG_VCALL_IV:
		// DES and AES calls send it (TS 1-B 4.1.5): the call is encrypted whatever else is lost
		if (c->active) {
			c->call.iv = true;
#if CONFIG_AIR_AES
			// The header's IV is the first session's. After it, a VCALL_IV carries the next session's IV (TS 1-D
			// 5.4.4.2 (4)): in the SACCH of each session's second superframe, so a late entry's next frame
			// starts a session; a FACCH1 one (9600 bit/s) does not say where in the session it is
			if (head) {
				(void)nxdn_viv_parse(msg, c->call.iv_val);
				c->anchored = true;
				c->vch_n = 0;
			} else if (c->anchored || sacch) {
				(void)nxdn_viv_parse(msg, c->next_iv);
				c->next_iv_valid = true;
				c->sync_next = !c->anchored && sacch;
			}
			aes_judge(c);
#endif
			if (c->sink->update)
				c->sink->update(&c->call, c->ctx);
		}
		break;
	case NXDN_MSG_TX_REL:
		call_end(c);
		break;
	default:
		break;
	}
}

// One SACCH of a superframe: its 18 bits go to their part of the message, which is whole once the four came in order
static void sf_part(nxdn_call_rx_t *c, const nxdn_rx_frame_t *f)
{
	uint8_t part = NXDN_SR_PART(f->sacch[0]);

	if (part == 0) {
		memset(c->sf, 0, sizeof c->sf);
		c->sf_parts = 0;
	} else if (part != c->sf_next) {
		c->sf_parts = 0;
		c->sf_next = -1;
		return;
	}
	for (uint8_t i = 0; i < 18; i++)
		bit_put(c->sf, (uint16_t)(part * 18 + i), bit_get(f->sacch, (uint16_t)(8 + i)));
	c->sf_parts |= (uint8_t)(1u << part);
	c->sf_next = (int8_t)(part + 1);
	if (part == 3 && c->sf_parts == 0x0F) {
		c->sf_next = -1;
		on_msg(c, f, c->sf, false, true);
	}
}

static void on_frame(const nxdn_rx_frame_t *f, void *ctx)
{
	nxdn_call_rx_t *c = ctx;
	// A frame placed without its LICH continues the superframe: four voice channels
	int16_t lich = f->lich >= 0 ? f->lich : NXDN_LICH(NXDN_RF_RDCH, NXDN_USC_SACCH_SF, NXDN_STEAL_NONE, 0);
	uint8_t usc = NXDN_LICH_USC(lich), opt = NXDN_LICH_OPT(lich), msg[NXDN_FACCH1_BYTES];
	bool sf = usc == NXDN_USC_SACCH_SF || usc == NXDN_USC_SACCH_SF_IDLE;

	c->stats.frames++;
	if (f->lich < 0)
		c->stats.lich_fail++;
	if (f->rssi_dbm)
		c->call.rssi_dbm = f->rssi_dbm;
	if (f->sacch_errs < 0) {
		c->stats.sacch_fail++;
		c->sf_parts = 0;
		c->sf_next = -1;
	} else {
		c->stats.fec_bits += (uint32_t)f->sacch_errs;
		if (sf) {
			// A late entry: the call is there, its fields come with the superframe's last SACCH
			if (!c->active)
				call_start(c, f, true);
			sf_part(c, f);
		} else {
			c->sf_parts = 0;
			c->sf_next = -1;
		}
	}
	// FACCH1 in the halves the LICH names; an idle FACCH1 frame carries nothing (TS 1-B 4.1.2.1)
	for (uint8_t h = 0; h < 2; h++) {
		int errs;

		if (opt == NXDN_STEAL_NONE || (opt == NXDN_STEAL_FIRST && h == 1) || (opt == NXDN_STEAL_SECOND && h == 0))
			continue;
		if (usc == NXDN_USC_SACCH_SF_IDLE && opt == NXDN_STEAL_BOTH)
			continue;
		errs = nxdn_facch1_get(f->hard, f->soft, h, &c->rx.work, msg);
		if (errs < 0) {
			c->stats.facch_fail++;
			continue;
		}
		c->stats.fec_bits += (uint32_t)errs;
		// (both halves are read: an encrypted call's header carries its VCALL_IV in the second, TS 1-B 4.1.5)
		on_msg(c, f, msg, !sf, false);
	}
	// Voice in the halves not taken
	for (uint8_t n = 0; n < NXDN_VOICE_FRAMES; n++) {
		uint8_t ehr[NXDN_EHR_BYTES];

		if (opt == NXDN_STEAL_BOTH || (opt == NXDN_STEAL_FIRST && n < 2) || (opt == NXDN_STEAL_SECOND && n >= 2))
			continue;
		if (!c->active || !playable(&c->call)) {
			c->stats.voice_held++;
			continue;
		}
		nxdn_vch_get(f->hard, n, ehr);
#if CONFIG_AIR_AES
		if (c->call.decrypt)
			nxdn_aes_vch(c->aes_key, c->call.iv_val, c->vch_n + n, ehr);
#endif
		c->stats.voice++;
		if (c->sink->voice)
			c->sink->voice(ehr, c->ctx);
	}
#if CONFIG_AIR_AES
	if (sf && c->active) {
		if (c->anchored)
			c->vch_n += NXDN_VOICE_FRAMES;
		if (c->vch_n == NXDN_AES_SESSION_VCH || c->sync_next) {
			if (c->next_iv_valid)
				memcpy(c->call.iv_val, c->next_iv, 8);
			else
				aes_voice_iv_next(c->call.iv_val);
			c->vch_n = 0;
			c->anchored = true;
			c->next_iv_valid = c->sync_next = false;
			if (aes_judge(c) && c->sink->update)
				c->sink->update(&c->call, c->ctx);
		}
	}
#endif
}

static void on_lost(void *ctx)
{
	call_end(ctx);
}

void nxdn_call_rx_init(nxdn_call_rx_t *c, uint8_t ran, const nxdn_call_sink_t *sink, void *ctx)
{
	memset(c, 0, sizeof *c);
	c->sink = sink;
	c->ctx = ctx;
	c->sf_next = -1;
	// (on_frame's context is c; the framer's is c->rx, the same address)
	nxdn_rx_init(&c->rx, ran, on_frame, on_lost, c);
}

// ── Transmit (TS 1-B 4.1.3, figure 4.1-7) ──
enum { TX_PRE, TX_HEAD, TX_SF, TX_END, TX_DONE };

// A FACCH1 frame of a superframe at 9600 bit/s: the second and fourth (TS 1-B 4.1.2.1)
static bool facch_frame(const nxdn_call_tx_t *t)
{
	return t->rate9600 && (t->sf_frame & 1);
}

// The SACCH: SR, then 18 bits of msg from bit `from` (an IDLE message: from = 0 of a one-octet IDLE)
static void sacch(nxdn_call_tx_t *t, uint8_t part, const uint8_t *msg, uint16_t from)
{
	uint8_t in[NXDN_SACCH_BYTES] = { NXDN_SR(part, t->ran), 0, 0, 0 };

	for (uint8_t i = 0; i < 18; i++)
		bit_put(in, (uint16_t)(8 + i), bit_get(msg, (uint16_t)(from + i)));
	nxdn_sacch_put(t->frame, in);
}

// The second FACCH1 of a header and of a frame that carries FACCH1: the VCALL again, or an encrypted call's VCALL_IV
static const uint8_t *tx_second(const nxdn_call_tx_t *t)
{
#if CONFIG_AIR_AES
	if (t->aes_key != NULL)
		return t->viv;
#endif
	return t->msg;
}

static void tx_frame(nxdn_call_tx_t *t, uint8_t state)
{
	static const uint8_t idle[3] = { NXDN_MSG_IDLE, 0, 0 };

	t->state = state;
	t->sent = 0;
	t->nvoice = 0;
	switch (state) {
	case TX_PRE:
		memcpy(t->frame, nxdn_preamble, sizeof nxdn_preamble);
		t->nsyms = t->ready = NXDN_PREAMBLE_SYMS;
		break;
	case TX_HEAD:
	case TX_END:
		// Non-superframe: SACCH IDLE alone (structure 4/4), the message twice in FACCH1
		nxdn_frame_begin(t->frame, NXDN_LICH(NXDN_RF_RDCH, NXDN_USC_SACCH_NS, NXDN_STEAL_BOTH, t->dir));
		sacch(t, 3, idle, 0);
		nxdn_facch1_put(t->frame, 0, state == TX_HEAD ? t->msg : t->rel);
		nxdn_facch1_put(t->frame, 1, state == TX_HEAD ? tx_second(t) : t->rel);
		t->nsyms = t->ready = NXDN_FRAME_SYMS;
		break;
	case TX_SF:
		// SACCH part k of the VCALL (structure 1/4 .. 4/4)
		t->nsyms = NXDN_FRAME_SYMS;
#if CONFIG_AIR_AES
		const uint8_t *sf_msg = t->aes_key != NULL && t->vch_base >= NXDN_AES_SESSION_VCH / 2 ? t->viv : t->msg;
#else
		const uint8_t *sf_msg = t->msg;
#endif
		if (facch_frame(t)) {
			// (what a FACCH1 frame carries in a call is open: the VCALL again, nxdn.md 9)
			nxdn_frame_begin(t->frame, NXDN_LICH(NXDN_RF_RDCH, NXDN_USC_SACCH_SF, NXDN_STEAL_BOTH, t->dir));
			sacch(t, t->sf_frame, sf_msg, (uint16_t)(18 * t->sf_frame));
			nxdn_facch1_put(t->frame, 0, t->msg);
			nxdn_facch1_put(t->frame, 1, tx_second(t));
			t->ready = NXDN_FRAME_SYMS;
		} else {
			// Nothing of a voice frame goes out before its first VCH: an end that comes first sends the release
			// frame right after the frame before
			nxdn_frame_begin(t->frame, NXDN_LICH(NXDN_RF_RDCH, NXDN_USC_SACCH_SF, NXDN_STEAL_NONE, t->dir));
			sacch(t, t->sf_frame, sf_msg, (uint16_t)(18 * t->sf_frame));
			t->ready = 0;
		}
		break;
	default:
		t->nsyms = t->ready = 0;
		break;
	}
}

void nxdn_call_tx_begin(nxdn_call_tx_t *t, const nxdn_vcall_t *v, uint8_t ran, uint8_t dir, bool rate9600)
{
	nxdn_vcall_t rel = *v;

	memset(t, 0, sizeof *t);
	t->ran = ran;
	t->dir = dir;
	t->rate9600 = rate9600;
	nxdn_vcall_build(v, t->msg, sizeof t->msg);
	rel.msg = NXDN_MSG_TX_REL;
	nxdn_vcall_build(&rel, t->rel, sizeof t->rel);
	tx_frame(t, TX_PRE);
}

#if CONFIG_AIR_AES
void nxdn_call_tx_begin_enc(nxdn_call_tx_t *t, const nxdn_vcall_t *v, uint8_t ran, uint8_t dir, bool rate9600,
			    const uint8_t *key, const uint8_t iv[8])
{
	nxdn_vcall_t rel = *v;

	memset(t, 0, sizeof *t);
	t->ran = ran;
	t->dir = dir;
	t->rate9600 = rate9600;
	t->aes_key = key;
	memcpy(t->iv, iv, sizeof t->iv);
	nxdn_viv_build(t->iv, t->viv);
	nxdn_vcall_build(v, t->msg, sizeof t->msg);
	rel.msg = NXDN_MSG_TX_REL;
	nxdn_vcall_build(&rel, t->rel, sizeof t->rel);
	tx_frame(t, TX_PRE);
}
#endif

uint16_t nxdn_call_tx_ready(const nxdn_call_tx_t *t, uint16_t *first)
{
	*first = t->sent;
	return (uint16_t)(t->ready - t->sent);
}

const uint8_t *nxdn_call_tx_syms(const nxdn_call_tx_t *t)
{
	return t->frame;
}

void nxdn_call_tx_sent(nxdn_call_tx_t *t, uint16_t n)
{
	t->sent = (uint16_t)(t->sent + n);
	if (t->sent < t->nsyms || t->state == TX_DONE)
		return;
	switch (t->state) {
	case TX_PRE:
		tx_frame(t, TX_HEAD);
		break;
	case TX_HEAD:
		t->sf_frame = 0;
#if CONFIG_AIR_AES
		if (t->aes_key != NULL) {
			uint8_t next[8];
			memcpy(next, t->iv, 8);
			aes_voice_iv_next(next);
			nxdn_viv_build(next, t->viv);
		}
#endif
		tx_frame(t, t->ending ? TX_END : TX_SF);
		break;
	case TX_SF:
		// A release may follow any frame of a superframe (TS 1-B 4.1.3)
		t->sf_frame = (uint8_t)((t->sf_frame + 1) & 3);
#if CONFIG_AIR_AES
		t->vch_base += NXDN_VOICE_FRAMES;	// FACCH1 keeps its VCH positions
		if (t->aes_key != NULL && t->vch_base == NXDN_AES_SESSION_VCH) {
			uint8_t next[8];
			aes_voice_iv_next(t->iv);
			memcpy(next, t->iv, 8);
			aes_voice_iv_next(next);
			nxdn_viv_build(next, t->viv);
			t->vch_base = 0;
		}
#endif
		tx_frame(t, t->ending ? TX_END : TX_SF);
		break;
	default:
		tx_frame(t, TX_DONE);
		break;
	}
}

bool nxdn_call_tx_wants_voice(const nxdn_call_tx_t *t)
{
	return t->state == TX_SF && !facch_frame(t) && t->nvoice < NXDN_VOICE_FRAMES;
}

void nxdn_call_tx_voice(nxdn_call_tx_t *t, const uint8_t ehr[NXDN_EHR_BYTES])
{
	if (!nxdn_call_tx_wants_voice(t))
		return;
#if CONFIG_AIR_AES
	if (t->aes_key != NULL) {
		uint8_t enc[NXDN_EHR_BYTES];

		memcpy(enc, ehr, sizeof enc);
		nxdn_aes_vch(t->aes_key, t->iv, t->vch_base + t->nvoice, enc);
		nxdn_vch_put(t->frame, t->nvoice, enc);
	} else
#endif
	nxdn_vch_put(t->frame, t->nvoice, ehr);
	t->nvoice++;
	t->ready = NXDN_VCH_SYM(t->nvoice);
}

void nxdn_call_tx_end(nxdn_call_tx_t *t, const uint8_t silence[NXDN_EHR_BYTES])
{
	t->ending = true;
	// A superframe frame with nothing on air yet is not sent at all
	if (t->state == TX_SF && t->sent == 0 && (facch_frame(t) || t->nvoice == 0)) {
		tx_frame(t, TX_END);
		return;
	}
	while (nxdn_call_tx_wants_voice(t))
		nxdn_call_tx_voice(t, silence);
}

bool nxdn_call_tx_done(const nxdn_call_tx_t *t)
{
	return t->state == TX_DONE;
}
