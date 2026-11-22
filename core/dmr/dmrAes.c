// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// DMR AES-256 voice (dmrAes.h)
#include "config.h"
#if CONFIG_AIR_AES && CONFIG_DMR_PRIVACY	// the whole file
#include "dmrAes.h"
#include "crypto/aes256.h"
#include "dmr/ambe/ambeFec.h"

#include <string.h>

static void wipe(uint8_t *bytes, unsigned size)
{
	volatile uint8_t *p = bytes;
	while (size--)
		*p++ = 0;
}

static uint32_t lfsr_bit(uint32_t m)
{
	return ((m >> 31) ^ (m >> 21) ^ (m >> 1) ^ m) & 1u;
}

uint32_t dmrAesMiNext(uint32_t mi)
{
	for (unsigned i = 0; i < 32; i++)
		mi = (mi << 1) | lfsr_bit(mi);
	return mi;
}

uint32_t dmrVoiceCipherNextMi(uint8_t alg, uint32_t mi)
{
	if (alg == DMR_AES_ALG)
		return dmrAesMiNext(mi);
	for (unsigned i = 0; i < 32; i++) {
		uint32_t bit = ((mi >> 31) ^ (mi >> 3) ^ (mi >> 1)) & 1u;
		mi = (mi << 1) | bit;
	}
	return mi;
}

// Motorola ARC4 keys the stream with the five key octets followed by the big-endian MI.
// Drop 256 bytes, then use seven per AMBE frame, including erased or silent frames.
static void arc4_generate(dmr_aes_t *a)
{
	uint8_t state[256], seed[DMR_ARC4_KEY_BYTES + 4];
	memcpy(seed, a->key, DMR_ARC4_KEY_BYTES);
	for (unsigned i = 0; i < 4; i++)
		seed[DMR_ARC4_KEY_BYTES + i] = (uint8_t)(a->mi >> (24 - 8 * i));
	for (unsigned i = 0; i < sizeof state; i++)
		state[i] = (uint8_t)i;
	uint8_t j = 0;
	for (unsigned i = 0; i < sizeof state; i++) {
		uint8_t x = state[i];
		j = (uint8_t)(j + x + seed[i % sizeof seed]);
		state[i] = state[j];
		state[j] = x;
	}
	j = 0;
	for (unsigned n = 0; n < 256 + DMR_AES_SF_BYTES; n++) {
		uint8_t i = (uint8_t)(n + 1);
		uint8_t x = state[i];
		j = (uint8_t)(j + x);
		uint8_t y = state[j];
		state[i] = y;
		state[j] = x;
		if (n >= 256)
			a->ks[n - 256] = state[(uint8_t)(x + y)];
	}
	wipe(state, sizeof state);
	wipe(seed, sizeof seed);
}

// The superframe's keystream: IV = the MI, then 96 bits more of its LFSR
static void generate(dmr_aes_t *a)
{
	if (a->alg == DMR_ARC4_ALG) {
		arc4_generate(a);
		return;
	}
	uint8_t blk[AES_BLOCK_BYTES] = { 0 };
	uint32_t m = a->mi;

	for (unsigned i = 0; i < 4; i++)
		blk[i] = (uint8_t)(a->mi >> (24 - 8 * i));
	for (unsigned i = 32; i < 128; i++) {
		uint32_t b = lfsr_bit(m);

		m = (m << 1) | b;
		blk[i / 8] = (uint8_t)((blk[i / 8] << 1) | b);
	}
	aes256_encrypt_block(a->key, blk, blk);	// the first output block only feeds the next one
	for (unsigned at = 0; at < DMR_AES_SF_BYTES; at += AES_BLOCK_BYTES) {
		aes256_encrypt_block(a->key, blk, blk);
		memcpy(&a->ks[at], blk, DMR_AES_SF_BYTES - at < AES_BLOCK_BYTES ? DMR_AES_SF_BYTES - at : AES_BLOCK_BYTES);
	}
	memset(blk, 0, sizeof blk);
}

void dmrAesStart(dmr_aes_t *a, const uint8_t *key, uint32_t mi)
{
	dmrVoiceCipherStart(a, DMR_AES_ALG, key, mi);
}

void dmrVoiceCipherStart(dmr_aes_t *a, uint8_t alg, const uint8_t *key, uint32_t mi)
{
	memset(a, 0, sizeof *a);
	a->key = key;
	a->mi = mi;
	a->pos = 0xFF;
	a->alg = alg;
	a->ready = key != NULL && mi != 0 && (alg == DMR_AES_ALG || alg == DMR_ARC4_ALG);
	if (a->ready)
		generate(a);
}

void dmrAesStop(dmr_aes_t *a)
{
	wipe((uint8_t *)a, sizeof *a);
	a->pos = 0xFF;
}

bool dmrAesPiParse(const uint8_t pi[DMR_AES_PI_BYTES], uint8_t *kid, uint32_t *mi)
{
	return dmrVoiceCipherPiParse(pi, DMR_AES_ALG, kid, mi);
}

bool dmrVoiceCipherPiParse(const uint8_t pi[DMR_AES_PI_BYTES], uint8_t alg, uint8_t *kid, uint32_t *mi)
{
	*kid = pi[2];
	*mi = (uint32_t)pi[3] << 24 | (uint32_t)pi[4] << 16 | (uint32_t)pi[5] << 8 | pi[6];
	return pi[1] == DMR_AES_FID && (pi[0] & 0xDFu) == (alg & 0xDFu) && *kid != 0 && *mi != 0;
}

void dmrAesPiBuild(uint8_t kid, uint32_t mi, uint32_t dst, uint8_t pi[DMR_AES_PI_BYTES])
{
	dmrVoiceCipherPiBuild(DMR_AES_ALG, kid, mi, dst, pi);
}

void dmrVoiceCipherPiBuild(uint8_t alg, uint8_t kid, uint32_t mi, uint32_t dst, uint8_t pi[DMR_AES_PI_BYTES])
{
	pi[0] = alg;
	pi[1] = DMR_AES_FID;
	pi[2] = kid;
	for (unsigned i = 0; i < 4; i++)
		pi[3 + i] = (uint8_t)(mi >> (24 - 8 * i));
	pi[7] = (uint8_t)(dst >> 16);
	pi[8] = (uint8_t)(dst >> 8);
	pi[9] = (uint8_t)dst;
}

// Superframe boundaries crossed from the last burst to this one. Bursts come in batches and late, so time is no
// clock: the slot counter is (a slot's bursts are every other tick), else the index alone (the same index again is a
// superframe on: the call stack has dropped the repeats)
static unsigned crossed(const dmr_aes_t *a, uint8_t index, uint32_t slot)
{
	if (a->pos > 5)
		return 0;

	unsigned r = (unsigned)(index + 6 - a->pos) % 6, k = 0;

	if (slot && a->slot) {
		uint32_t bursts = (slot - a->slot + 1) / 2;

		if (bursts > r)
			k = (unsigned)((bursts - r + 3) / 6);
	}
	if (!r && !k)
		k = 1;
	return (a->pos + r + 6 * k) / 6;
}

bool dmrAesBurst(dmr_aes_t *a, uint8_t ambe27[27], uint8_t index, uint32_t slot, const uint8_t *silence)
{
	if (!a->ready || index > 5)
		return false;

	unsigned n = crossed(a, index, slot);

	if (n > DMR_AES_MAX_GAP) {
		dmrAesStop(a);
		return false;
	}
	if (n) {
		while (n--)
			a->mi = dmrVoiceCipherNextMi(a->alg, a->mi);
		generate(a);
	}
	for (unsigned f = 0; f < 3; f++) {
		uint8_t *fr = &ambe27[9 * f], bits[AMBE_PARAM_BITS];
		const uint8_t *k = &a->ks[(index * 3u + f) * 7u];
		ambe_fec_result_t r;
		bool quiet = silence && !memcmp(fr, silence, 9);

		ambeFecDecode(fr, bits, &r);
		if (silence && (quiet || r.c0_failed)) {
			memcpy(fr, silence, 9);
			continue;
		}
		for (unsigned b = 0; b < AMBE_PARAM_BITS; b++)
			bits[b] ^= (uint8_t)((k[b / 8] >> (7 - b % 8)) & 1u);
		ambeFecEncode(bits, fr);
		memset(bits, 0, sizeof bits);
	}
	a->pos = index;
	a->slot = slot;
	return true;
}

// --- AES-256 late entry (ETSI TS 102 361-1 9.3.3, the encryption sync) ---
//
// A receiver that joins a call after its PI header can still decrypt it: every voice superframe carries the NEXT
// superframe's MI, and burst F carries the key ID and algorithm as a single burst. The MI rides in the AMBE
// frames' least significant bits, four per frame: the first 12 bits of each Golay(24,12) codeword go in bursts
// A-C, the last 12 in D-F, so a superframe's 18 nibbles hold three codewords carrying the 32-bit MI and a 4-bit
// CRC. The MI conveyed is the next superframe's, never the current one: a receiver that took the current MI
// would decrypt one superframe early and garble it.
//
// The nibbles sit at the interleaved AMBE positions 71, 67, 63, 59 of each 9-octet frame (the low four bits of
// the frame's last FEC word, which no FEC protects): nibble bit 3 in octet 8 bit 0, bit 2 in octet 8 bit 4, bit
// 1 in octet 7 bit 0, bit 0 in octet 7 bit 4. The codewords are the binary Golay(24,12,8) code, generator and
// parity-check matrices below (ETSI TS 102 361-1 B.3).

static const uint8_t LE_GOLAY_G[24 * 12] = {
	1,0,0,0,0,0,0,0,0,0,0,0, 1,1,0,0,0,1,1,1,0,1,0,1,
	0,1,0,0,0,0,0,0,0,0,0,0, 0,1,1,0,0,0,1,1,1,0,1,1,
	0,0,1,0,0,0,0,0,0,0,0,0, 1,1,1,1,0,1,1,0,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0, 0,1,1,1,1,0,1,1,0,1,0,0,
	0,0,0,0,1,0,0,0,0,0,0,0, 0,0,1,1,1,1,0,1,1,0,1,0,
	0,0,0,0,0,1,0,0,0,0,0,0, 1,1,0,1,1,0,0,1,1,0,0,1,
	0,0,0,0,0,0,1,0,0,0,0,0, 0,1,1,0,1,1,0,0,1,1,0,1,
	0,0,0,0,0,0,0,1,0,0,0,0, 0,0,1,1,0,1,1,0,0,1,1,1,
	0,0,0,0,0,0,0,0,1,0,0,0, 1,1,0,1,1,1,0,0,0,1,1,0,
	0,0,0,0,0,0,0,0,0,1,0,0, 1,0,1,0,1,0,0,1,0,1,1,1,
	0,0,0,0,0,0,0,0,0,0,1,0, 1,0,0,1,0,0,1,1,1,1,1,0,
	0,0,0,0,0,0,0,0,0,0,0,1, 1,0,0,0,1,1,1,0,1,0,1,1,
};

static const uint8_t LE_GOLAY_H[24 * 12] = {
	1,0,1,0,0,1,0,0,1,1,1,1, 1,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,0,1,1,0,1,0,0,0, 0,1,0,0,0,0,0,0,0,0,0,0,
	0,1,1,1,1,0,1,1,0,1,0,0, 0,0,1,0,0,0,0,0,0,0,0,0,
	0,0,1,1,1,1,0,1,1,0,1,0, 0,0,0,1,0,0,0,0,0,0,0,0,
	0,0,0,1,1,1,1,0,1,1,0,1, 0,0,0,0,1,0,0,0,0,0,0,0,
	1,0,1,0,1,0,1,1,1,0,0,1, 0,0,0,0,0,1,0,0,0,0,0,0,
	1,1,1,1,0,0,0,1,0,0,1,1, 0,0,0,0,0,0,1,0,0,0,0,0,
	1,1,0,1,1,1,0,0,0,1,1,0, 0,0,0,0,0,0,0,1,0,0,0,0,
	0,1,1,0,1,1,1,0,0,0,1,1, 0,0,0,0,0,0,0,0,1,0,0,0,
	1,0,0,1,0,0,1,1,1,1,1,0, 0,0,0,0,0,0,0,0,0,1,0,0,
	0,1,0,0,1,0,0,1,1,1,1,1, 0,0,0,0,0,0,0,0,0,0,1,0,
	1,1,0,0,0,1,1,1,0,1,0,1, 0,0,0,0,0,0,0,0,0,0,0,1,
};

static uint8_t le_crc4(const uint8_t *bits, unsigned len)
{
	// x^4 + x + 1 over the bits, MSB first, the remainder inverted
	static const uint8_t poly[5] = { 1, 0, 0, 1, 1 };
	uint8_t buf[40];
	uint8_t crc = 0;

	if (len + 4 > sizeof buf)
		return 0;
	memset(buf, 0, sizeof buf);
	for (unsigned i = 0; i < len; i++)
		buf[i] = bits[i];
	for (unsigned i = 0; i < len; i++)
		if (buf[i])
			for (unsigned j = 0; j < 5; j++)
				buf[i + j] ^= poly[j];
	for (unsigned i = 0; i < 4; i++)
		crc = (uint8_t)((crc << 1) + buf[len + i]);
	return (uint8_t)(crc ^ 0x0F);
}

static void le_golay_encode(const uint8_t in12[12], uint8_t out24[24])
{
	for (int j = 0; j < 24; j++) {
		unsigned s = 0;

		for (int i = 0; i < 12; i++)
			s += (unsigned)in12[i] * LE_GOLAY_G[24 * i + j];
		out24[j] = (uint8_t)(s & 1u);
	}
}

// Correct one 24-bit Golay word in place. The minimum distance is 8, so every error pattern of weight 3 or less
// has a unique syndrome: a search over the single-error syndromes and their pairs and triples finds it without a
// table. Returns the errors corrected (0-3), or -1 when uncorrectable
static int le_golay_correct(uint8_t cw[24])
{
	unsigned col[24];
	unsigned s = 0;

	for (int j = 0; j < 24; j++) {
		unsigned cj = 0;

		for (int r = 0; r < 12; r++)
			cj |= (unsigned)(LE_GOLAY_H[24 * r + j] & 1u) << (11 - r);
		col[j] = cj;
	}
	for (int r = 0; r < 12; r++) {
		unsigned acc = 0;

		for (int j = 0; j < 24; j++)
			acc += (unsigned)cw[j] * LE_GOLAY_H[24 * r + j];
		s |= (acc & 1u) << (11 - r);
	}
	if (s == 0)
		return 0;
	for (int a = 0; a < 24; a++)
		if (col[a] == s) {
			cw[a] ^= 1;
			return 1;
		}
	for (int a = 0; a < 24; a++)
		for (int b = a + 1; b < 24; b++)
			if ((col[a] ^ col[b]) == s) {
				cw[a] ^= 1;
				cw[b] ^= 1;
				return 2;
			}
	for (int a = 0; a < 24; a++)
		for (int b = a + 1; b < 24; b++)
			for (int c = b + 1; c < 24; c++)
				if ((col[a] ^ col[b] ^ col[c]) == s) {
					cw[a] ^= 1;
					cw[b] ^= 1;
					cw[c] ^= 1;
					return 3;
				}
	return -1;
}

void dmrAesLeBuild(uint32_t mi, uint8_t frag[6][3])
{
	uint8_t bits[36];
	uint64_t mi_test = 0, go_test = 0;

	for (int i = 0; i < 32; i++)
		bits[i] = (uint8_t)((mi >> (31 - i)) & 1u);
	uint8_t crc = le_crc4(bits, 32);

	for (int b = 0; b < 4; b++)
		bits[32 + b] = (uint8_t)((crc >> (3 - b)) & 1u);
	for (int j = 0; j < 3; j++) {
		uint8_t enc[24];

		le_golay_encode(&bits[j * 12], enc);
		for (int i = 0; i < 12; i++) {
			int pos = 35 - (i + j * 12);

			mi_test |= (uint64_t)(enc[i] & 1u) << pos;
			go_test |= (uint64_t)(enc[i + 12] & 1u) << pos;
		}
	}
	// Bursts A-C carry the codewords' first halves, D-F the second halves, one nibble per codeword per burst
	frag[0][0] = (uint8_t)((mi_test >> 32) & 0xF);
	frag[1][0] = (uint8_t)((mi_test >> 28) & 0xF);
	frag[2][0] = (uint8_t)((mi_test >> 24) & 0xF);
	frag[0][1] = (uint8_t)((mi_test >> 20) & 0xF);
	frag[1][1] = (uint8_t)((mi_test >> 16) & 0xF);
	frag[2][1] = (uint8_t)((mi_test >> 12) & 0xF);
	frag[0][2] = (uint8_t)((mi_test >> 8) & 0xF);
	frag[1][2] = (uint8_t)((mi_test >> 4) & 0xF);
	frag[2][2] = (uint8_t)(mi_test & 0xF);
	frag[3][0] = (uint8_t)((go_test >> 32) & 0xF);
	frag[4][0] = (uint8_t)((go_test >> 28) & 0xF);
	frag[5][0] = (uint8_t)((go_test >> 24) & 0xF);
	frag[3][1] = (uint8_t)((go_test >> 20) & 0xF);
	frag[4][1] = (uint8_t)((go_test >> 16) & 0xF);
	frag[5][1] = (uint8_t)((go_test >> 12) & 0xF);
	frag[3][2] = (uint8_t)((go_test >> 8) & 0xF);
	frag[4][2] = (uint8_t)((go_test >> 4) & 0xF);
	frag[5][2] = (uint8_t)(go_test & 0xF);
}

bool dmrAesLeDecode(const uint8_t frag[6][3], uint32_t *mi)
{
	uint64_t mi_test = ((uint64_t)frag[0][0] << 32) | ((uint64_t)frag[1][0] << 28) |
			   ((uint64_t)frag[2][0] << 24) | ((uint64_t)frag[0][1] << 20) |
			   ((uint64_t)frag[1][1] << 16) | ((uint64_t)frag[2][1] << 12) |
			   ((uint64_t)frag[0][2] << 8) | ((uint64_t)frag[1][2] << 4) | frag[2][2];
	uint64_t go_test = ((uint64_t)frag[3][0] << 32) | ((uint64_t)frag[4][0] << 28) |
			   ((uint64_t)frag[5][0] << 24) | ((uint64_t)frag[3][1] << 20) |
			   ((uint64_t)frag[4][1] << 16) | ((uint64_t)frag[5][1] << 12) |
			   ((uint64_t)frag[3][2] << 8) | ((uint64_t)frag[4][2] << 4) | frag[5][2];
	uint8_t bits[36];
	bool good = true;

	for (int j = 0; j < 3; j++) {
		uint8_t cw[24];

		for (int i = 0; i < 12; i++) {
			cw[i] = (uint8_t)(((mi_test << (i + j * 12)) & 0x800000000ULL) >> 35);
			cw[i + 12] = (uint8_t)(((go_test << (i + j * 12)) & 0x800000000ULL) >> 35);
		}
		if (le_golay_correct(cw) < 0)
			good = false;
		for (int i = 0; i < 12; i++)
			bits[i + j * 12] = cw[i];
	}
	uint32_t out = 0;

	for (int i = 0; i < 32; i++)
		out = (out << 1) | bits[i];
	uint8_t crc_ext = 0;

	for (int i = 0; i < 4; i++)
		crc_ext = (uint8_t)((crc_ext << 1) | bits[32 + i]);
	if (!good || crc_ext != le_crc4(bits, 32))
		return false;
	*mi = out;
	return true;
}

// One fragment nibble in a 9-octet AMBE frame: bit 3 in octet 8 bit 0, bit 2 in octet 8 bit 4, bit 1 in octet 7
// bit 0, bit 0 in octet 7 bit 4
static uint8_t le_nibble_get(const uint8_t fr[9])
{
	return (uint8_t)(((fr[8] & 0x01u) << 3) | ((fr[8] & 0x10u) >> 2) | ((fr[7] & 0x01u) << 1) |
			 ((fr[7] & 0x10u) >> 4));
}

static void le_nibble_set(uint8_t fr[9], uint8_t nib)
{
	fr[8] = (uint8_t)((fr[8] & ~0x11u) | ((nib & 0x08u) >> 3) | ((nib & 0x04u) << 2));
	fr[7] = (uint8_t)((fr[7] & ~0x11u) | ((nib & 0x02u) >> 1) | ((nib & 0x01u) << 4));
}

void dmrAesLeStuff(uint8_t v[27], uint8_t index, const uint8_t frag[6][3])
{
	if (index > 5)
		return;
	for (int f = 0; f < 3; f++)
		le_nibble_set(&v[9 * f], frag[index][f]);
}

void dmrAesLeCollect(const uint8_t v[27], uint8_t index, uint8_t frag[6][3])
{
	if (index > 5)
		return;
	for (int f = 0; f < 3; f++)
		frag[index][f] = le_nibble_get(&v[9 * f]);
}

#endif
