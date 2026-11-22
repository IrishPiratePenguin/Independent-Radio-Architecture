// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// AES-256 encryption and output feedback mode (aes256.h)
#include "aes256.h"

#include <string.h>

static const uint8_t sbox[256] = {
#include "aes_sbox.inc"
};

static uint8_t xtime(uint8_t a)
{
	return (uint8_t)((a << 1) ^ ((a & 0x80u) != 0 ? 0x1Bu : 0u));
}

// The key schedule on a window of two round keys, [A, B] = k[0..15], k[16..31] (FIPS 197 5.2 with Nk = 8). Round keys 0
// and 1 are the key. Round key 2m is A advanced with B: its first word takes SubWord(RotWord(B's last word)) and the
// round constant. Round key 2m + 1 is B advanced with the new A: its first word takes SubWord(A's last word). The other
// words of each add the word before
static void next_a(uint8_t k[32], uint8_t rcon)
{
	uint8_t t0 = (uint8_t)(sbox[k[29]] ^ rcon), t1 = sbox[k[30]], t2 = sbox[k[31]], t3 = sbox[k[28]];

	k[0] ^= t0;
	k[1] ^= t1;
	k[2] ^= t2;
	k[3] ^= t3;
	for (uint8_t i = 4; i < 16; i++)
		k[i] ^= k[i - 4];
}

static void next_b(uint8_t k[32])
{
	k[16] ^= sbox[k[12]];
	k[17] ^= sbox[k[13]];
	k[18] ^= sbox[k[14]];
	k[19] ^= sbox[k[15]];
	for (uint8_t i = 20; i < 32; i++)
		k[i] ^= k[i - 4];
}

static void add_key(uint8_t s[16], const uint8_t *rk)
{
	for (uint8_t i = 0; i < 16; i++)
		s[i] ^= rk[i];
}

// SubBytes and ShiftRows in one pass (the state is column major: s[4c + r] is row r of column c)
static void sub_shift(uint8_t s[16])
{
	uint8_t t[16];

	for (uint8_t c = 0; c < 4; c++)
		for (uint8_t r = 0; r < 4; r++)
			t[4 * c + r] = sbox[s[4 * ((c + r) & 3u) + r]];
	memcpy(s, t, 16);
}

// MixColumns: b0 = 2 a0 + 3 a1 + a2 + a3 = a0 + (a0 + a1 + a2 + a3) + 2 (a0 + a1), and so on round the column
static void mix_columns(uint8_t s[16])
{
	for (uint8_t c = 0; c < 16; c += 4) {
		uint8_t a0 = s[c], a1 = s[c + 1], a2 = s[c + 2], a3 = s[c + 3];
		uint8_t all = (uint8_t)(a0 ^ a1 ^ a2 ^ a3);

		s[c] = (uint8_t)(a0 ^ all ^ xtime((uint8_t)(a0 ^ a1)));
		s[c + 1] = (uint8_t)(a1 ^ all ^ xtime((uint8_t)(a1 ^ a2)));
		s[c + 2] = (uint8_t)(a2 ^ all ^ xtime((uint8_t)(a2 ^ a3)));
		s[c + 3] = (uint8_t)(a3 ^ all ^ xtime((uint8_t)(a3 ^ a0)));
	}
}

void aes256_encrypt_block(const uint8_t key[AES256_KEY_BYTES], const uint8_t in[AES_BLOCK_BYTES],
			  uint8_t out[AES_BLOCK_BYTES])
{
	uint8_t k[32], s[16], rcon = 1;

	memcpy(k, key, 32);
	memcpy(s, in, 16);
	add_key(s, k);			// round key 0
	for (uint8_t round = 1; round <= 14; round++) {
		sub_shift(s);
		if (round != 14)
			mix_columns(s);
		if ((round & 1u) != 0) {
			if (round != 1)
				next_b(k);
			add_key(s, k + 16);	// round key 1 is B as it came
		} else {
			next_a(k, rcon);
			rcon = xtime(rcon);
			add_key(s, k);
		}
	}
	memcpy(out, s, 16);
	memset(k, 0, sizeof k);
}

void aes_ofb_init(aes_ofb_t *o, const uint8_t key[AES256_KEY_BYTES], const uint8_t iv[AES_BLOCK_BYTES])
{
	memcpy(o->key, key, AES256_KEY_BYTES);
	aes256_encrypt_block(o->key, iv, o->reg);
	o->pos = 0;
}

static void ofb_next_block(aes_ofb_t *o)
{
	aes256_encrypt_block(o->key, o->reg, o->reg);
	o->pos = 0;
}

void aes_ofb_xor(aes_ofb_t *o, uint8_t *buf, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (o->pos == AES_BLOCK_BYTES)
			ofb_next_block(o);
		buf[i] ^= o->reg[o->pos++];
	}
}

void aes_ofb_skip(aes_ofb_t *o, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (o->pos == AES_BLOCK_BYTES)
			ofb_next_block(o);
		o->pos++;
	}
}

void aes_ofb_wipe(aes_ofb_t *o)
{
	memset(o, 0, sizeof *o);
}

void aes_voice_iv_next(uint8_t mi[8])
{
	uint64_t n = 0;

	for (unsigned i = 0; i < 8; i++)
		n = (n << 8) | mi[i];
	for (unsigned i = 0; i < 64; i++)
		n = (n << 1) | (((n >> 63) ^ (n >> 61) ^ (n >> 45) ^ (n >> 37) ^ (n >> 26) ^ (n >> 14)) & 1u);
	for (unsigned i = 0; i < 8; i++)
		mi[i] = (uint8_t)(n >> (56 - i * 8));
}
