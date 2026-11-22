// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Copyright (c) 2026 lyra XE1POC (changes)
// A conventional P25 voice call (p25.h): on receive, the units of p25_rx.c become a call with its fields and its
// voice frames in order; on transmit, the units are built and released symbol by symbol. Clause numbers are
// TIA-102.BAAA-A's.

#include "p25.h"

#include <string.h>

// ── Receive ──
static void call_start(p25_call_rx_t *c, const p25_rx_unit_t *u, bool late)
{
	memset(&c->call, 0, sizeof c->call);
	c->call.nac = u->nac;
	c->call.late = late;
	c->call.algid = P25_ALGID_CLEAR;
	c->call.rssi_dbm = u->rssi_dbm;
	c->active = true;
	c->stats.calls++;
#if CONFIG_AIR_AES
	c->mi_valid = false;
#endif
}

#if CONFIG_AIR_AES
// Whether this call's voice can be decrypted: AES-256 with the key id of the key we hold, and its indicator known
static bool aes_ready(const p25_call_rx_t *c)
{
	return c->aes_key != NULL && c->call.have_alg && c->call.algid == P25_ALGID_AES256 && c->call.kid == c->aes_kid &&
	       c->mi_valid;
}

void p25_call_rx_set_key(p25_call_rx_t *c, const uint8_t *key, uint16_t kid)
{
	c->aes_key = key;
	c->aes_kid = kid;
}

// HDU gives the first superframe's indicator; each LDU2 gives the following superframe's.
static void mi_take(p25_call_rx_t *c, const uint8_t *mi)
{
	if (!c->mi_valid || memcmp(c->mi, mi, P25_MI_CIPHER_BYTES) != 0) {
		memcpy(c->mi, mi, P25_MI_CIPHER_BYTES);
		c->mi_valid = p25_aes_mi_valid(c->mi);
	}
}
#endif

static void call_end(p25_call_rx_t *c)
{
	if (!c->active)
		return;
	c->active = false;
	if (c->sink->end)
		c->sink->end(c->ctx);
}

static void on_unit(const p25_rx_unit_t *u, void *ctx)
{
	p25_call_rx_t *c = ctx;
	uint8_t buf[P25_HDR_BYTES];
	bool fresh = false, changed = false;
#if CONFIG_AIR_AES
	uint8_t next_mi[P25_MI_CIPHER_BYTES];
	bool have_next = false;
#endif

	c->stats.units++;
#if CONFIG_P25_TRUNK
	if (c->sink->unit)
		c->sink->unit(u, c->ctx);
#endif
	switch (u->duid) {
	case P25_DUID_HDU:
		// A header opens a call, also on top of one whose terminator was missed
		call_end(c);
		call_start(c, u, false);
		fresh = true;
		if (p25_hdu_get(u->air, buf) >= 0) {
			c->call.have_alg = true;
			c->call.algid = buf[10];
			c->call.kid = (uint16_t)(buf[11] << 8 | buf[12]);
			c->call.lc.mfid = buf[9];
			c->call.lc.tgid = (uint16_t)(buf[13] << 8 | buf[14]);
#if CONFIG_AIR_AES
			if (c->call.algid == P25_ALGID_AES256)
				mi_take(c, buf);	// the first superframe's indicator
#endif
		}
		break;
	case P25_DUID_LDU1:
	case P25_DUID_LDU2:
		if (!c->active) {
			call_start(c, u, true);
			fresh = true;
		}
		if (u->duid == P25_DUID_LDU1) {
			uint8_t raw[P25_LC_BYTES];
			p25_lc_t lc;

			if (p25_ldu_lc_get(u->air, raw) < 0) {
				c->stats.lc_fail++;
			} else if (p25_lc_parse(raw, &lc)) {
				if (!c->call.have_lc || memcmp(&lc, &c->call.lc, sizeof lc))
					changed = true;
				c->call.lc = lc;
				c->call.have_lc = true;
			}
		} else {
			uint8_t es[P25_ES_BYTES];

			if (p25_ldu_es_get(u->air, es) < 0) {
				c->stats.es_fail++;
			} else {
				uint16_t kid = (uint16_t)(es[10] << 8 | es[11]);

				if (!c->call.have_alg || es[9] != c->call.algid || kid != c->call.kid) {
					changed = true;
#if CONFIG_AIR_AES
					c->mi_valid = false;
#endif
				}
				c->call.have_alg = true;
				c->call.algid = es[9];
				c->call.kid = kid;
#if CONFIG_AIR_AES
				if (es[9] == P25_ALGID_AES256) {
					memcpy(next_mi, es, sizeof next_mi);
					have_next = true;
				}
#endif
			}
		}
		break;
	case P25_DUID_TDU:
	case P25_DUID_TDULC:
		call_end(c);
		return;
	default:
		return;
	}
	if (u->rssi_dbm)
		c->call.rssi_dbm = u->rssi_dbm;
#if CONFIG_AIR_AES
	if (c->call.decrypt != aes_ready(c)) {
		c->call.decrypt = aes_ready(c);
		changed = true;
	}
#endif
	if (fresh) {
		if (c->sink->start)
			c->sink->start(&c->call, c->ctx);
	} else if (changed && c->sink->update) {
		c->sink->update(&c->call, c->ctx);
	}
	if (u->duid == P25_DUID_HDU)
		return;
	// Encrypted voice decodes to noise: it stays here. (A late entry learns the algorithm with its first LDU2;
	// an LDU1 before that is taken as clear, as a call without encryption sync is)
#if CONFIG_AIR_AES
	bool aes = false;
#endif
	if (c->call.algid != P25_ALGID_CLEAR) {
#if CONFIG_AIR_AES
		if (!c->call.decrypt)
			goto next_sync;
		p25_aes_start(&c->ofb, c->aes_key, c->mi, u->duid == P25_DUID_LDU2);
		aes = true;
#else
		return;
#endif
	}
	for (uint8_t n = 0; n < P25_VOICE_FRAMES; n++) {
		uint8_t imbe[P25_IMBE_BYTES];
		int errs = p25_ldu_voice_get(u->air, n, imbe);

#if CONFIG_AIR_AES
		if (aes) {
			if (n == 8)
				aes_ofb_skip(&c->ofb, 2);	// low-speed data precedes the ninth voice interval
			aes_ofb_xor(&c->ofb, imbe, P25_IMBE_BYTES);
		}
#endif
		c->stats.voice++;
		c->stats.voice_bits += (uint32_t)P25_VOICE_ERRS_TOTAL(errs);
		if (P25_VOICE_ERRS_BAD(errs))
			c->stats.voice_bad++;
		if (c->sink->voice)
			c->sink->voice(imbe, (uint8_t)errs, c->ctx);
	}
#if CONFIG_AIR_AES
next_sync:
	if (have_next)
		mi_take(c, next_mi);
	else if (aes && u->duid == P25_DUID_LDU2)
		p25_aes_mi_next(c->mi);		// the next LDU1 and LDU2 use the next indicator
#endif
}

static void on_lost(void *ctx)
{
	call_end(ctx);
}

void p25_call_rx_init(p25_call_rx_t *c, uint16_t nac, const p25_call_sink_t *sink, void *ctx)
{
	memset(c, 0, sizeof *c);
	c->sink = sink;
	c->ctx = ctx;
	// (on_unit's context is c; the framer's is c->rx, the same address)
	p25_rx_init(&c->rx, nac, on_unit, on_lost, c);
}

// ── Transmit ──
enum { TX_HDU, TX_LDU1, TX_LDU2, TX_TERM, TX_DONE };

static void tx_unit(p25_call_tx_t *t, uint8_t state)
{
	t->state = state;
	t->sent = 0;
	t->nvoice = 0;
	if (state == TX_HDU) {
		uint8_t hdr[P25_HDR_BYTES] = { 0 };	// clear voice: no message indicator

		hdr[10] = P25_ALGID_CLEAR;
#if CONFIG_AIR_AES
		if (t->aes_key != NULL) {
			memcpy(hdr, t->mi, P25_MI_CIPHER_BYTES);	// (the ninth octet of the indicator is 0)
			hdr[10] = P25_ALGID_AES256;
			hdr[11] = (uint8_t)(t->aes_kid >> 8);
			hdr[12] = (uint8_t)t->aes_kid;
		}
#endif
		hdr[13] = (uint8_t)(t->tgid >> 8);
		hdr[14] = (uint8_t)t->tgid;
		t->nsyms = t->ready = P25_HDU_SYMS;
		p25_unit_begin(t->unit, t->nsyms, t->nac, P25_DUID_HDU, t->ss);
		p25_hdu_put(t->unit, hdr);
	} else if (state == TX_LDU1 || state == TX_LDU2) {
		static const uint8_t lsd[2] = { 0, 0 };

		// Nothing of an LDU goes out before its first voice frame: an end that comes first sends the terminator
		// right after the LDU before (8.2.2), not an LDU of silence
		t->nsyms = P25_LDU_SYMS;
		t->ready = 0;
		p25_unit_begin(t->unit, t->nsyms, t->nac, state == TX_LDU1 ? P25_DUID_LDU1 : P25_DUID_LDU2, t->ss);
		if (state == TX_LDU1) {
			p25_ldu_lc_put(t->unit, t->lc);
#if CONFIG_AIR_AES
			if (t->aes_key != NULL)
				p25_aes_start(&t->ofb, t->aes_key, t->mi, false);
#endif
		} else {
			uint8_t es[P25_ES_BYTES] = { 0 };

			es[9] = P25_ALGID_CLEAR;
#if CONFIG_AIR_AES
			if (t->aes_key != NULL) {
				memcpy(es, t->mi, P25_MI_CIPHER_BYTES);
				p25_aes_mi_next(es);	// synchronization for the following superframe
				es[9] = P25_ALGID_AES256;
				es[10] = (uint8_t)(t->aes_kid >> 8);
				es[11] = (uint8_t)t->aes_kid;
				p25_aes_start(&t->ofb, t->aes_key, t->mi, true);
			}
#endif
			p25_ldu_es_put(t->unit, es);
		}
		p25_ldu_lsd_put(t->unit, lsd);
	} else if (state == TX_TERM) {
		t->nsyms = t->ready = P25_TDU_SYMS;
		p25_unit_begin(t->unit, t->nsyms, t->nac, P25_DUID_TDU, t->ss);
	} else {
		t->nsyms = t->ready = 0;
	}
}

void p25_call_tx_begin(p25_call_tx_t *t, uint16_t nac, const p25_lc_t *lc, uint8_t ss)
{
	memset(t, 0, sizeof *t);
	t->nac = nac;
	t->ss = ss;
	t->tgid = lc->lcf == P25_LCF_GROUP ? lc->tgid : 0;
	p25_lc_build(lc, t->lc);
	tx_unit(t, TX_HDU);
}

uint16_t p25_call_tx_ready(const p25_call_tx_t *t, uint16_t *first)
{
	*first = t->sent;
	return (uint16_t)(t->ready - t->sent);
}

void p25_call_tx_sent(p25_call_tx_t *t, uint16_t n)
{
	t->sent = (uint16_t)(t->sent + n);
	if (t->sent < t->nsyms || t->state == TX_DONE)
		return;
	switch (t->state) {
	case TX_HDU:
		tx_unit(t, t->ending ? TX_TERM : TX_LDU1);
		break;
	case TX_LDU1:
		// A terminator may follow either LDU (8.2.2)
		tx_unit(t, t->ending ? TX_TERM : TX_LDU2);
		break;
	case TX_LDU2:
#if CONFIG_AIR_AES
		if (t->aes_key != NULL)
			p25_aes_mi_next(t->mi);		// the next LDU1 and LDU2 use the next indicator
#endif
		tx_unit(t, t->ending ? TX_TERM : TX_LDU1);
		break;
	default:
		tx_unit(t, TX_DONE);
		break;
	}
}

#if CONFIG_AIR_AES
void p25_call_tx_begin_enc(p25_call_tx_t *t, uint16_t nac, const p25_lc_t *lc, uint8_t ss, const uint8_t *key,
			   uint16_t kid, const uint8_t mi[P25_MI_CIPHER_BYTES])
{
	memset(t, 0, sizeof *t);
	t->aes_key = key;
	t->aes_kid = kid;
	memcpy(t->mi, mi, P25_MI_CIPHER_BYTES);
	t->nac = nac;
	t->ss = ss;
	t->tgid = lc->lcf == P25_LCF_GROUP ? lc->tgid : 0;
	p25_lc_build(lc, t->lc);
	tx_unit(t, TX_HDU);
}
#endif

bool p25_call_tx_wants_voice(const p25_call_tx_t *t)
{
	return (t->state == TX_LDU1 || t->state == TX_LDU2) && t->nvoice < P25_VOICE_FRAMES;
}

void p25_call_tx_voice(p25_call_tx_t *t, const uint8_t imbe[P25_IMBE_BYTES])
{
	if (!p25_call_tx_wants_voice(t))
		return;
#if CONFIG_AIR_AES
	if (t->aes_key != NULL) {
		uint8_t enc[P25_IMBE_BYTES];

		memcpy(enc, imbe, sizeof enc);
		if (t->nvoice == 8)
			aes_ofb_skip(&t->ofb, 2);
		aes_ofb_xor(&t->ofb, enc, sizeof enc);	// the frame's 88 bits, before their FEC
		p25_ldu_voice_put(t->unit, t->nvoice, enc);
	} else
#endif
	p25_ldu_voice_put(t->unit, t->nvoice, imbe);
	t->nvoice++;
	t->ready = p25_ldu_voice_sym(t->nvoice);
}

void p25_call_tx_end(p25_call_tx_t *t, const uint8_t silence[P25_IMBE_BYTES])
{
	t->ending = true;
	// An LDU without a voice frame yet has nothing of it on air: not sent at all
	if ((t->state == TX_LDU1 || t->state == TX_LDU2) && t->nvoice == 0) {
		tx_unit(t, TX_TERM);
		return;
	}
	while (p25_call_tx_wants_voice(t))
		p25_call_tx_voice(t, silence);
}

bool p25_call_tx_done(const p25_call_tx_t *t)
{
	return t->state == TX_DONE;
}

#if CONFIG_P25_TRUNK
void p25_call_tx_single(p25_call_tx_t *t, uint16_t nsyms)
{
	// As a terminator: the unit goes out whole, then the transmission is done
	t->state = TX_TERM;
	t->ending = true;
	t->nvoice = 0;
	t->nsyms = t->ready = nsyms;
	t->sent = 0;
}
#endif
