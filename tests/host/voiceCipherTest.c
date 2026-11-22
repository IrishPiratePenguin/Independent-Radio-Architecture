// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// AES-256 voice keystreams against vectors this firmware did not make: P25 (TIA-102.AAAD-A layout) against the masks
// OP25 computes for one key and MI (as this fork's earlier patch series carried them), NXDN against the standard's
// own test vectors (TS 1-D 7.2.1.3 frames, TS 1-A 7.2.1.1 tone, TS 1-A 6.4.1.2 VCALL_IV).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "p25/p25.h"
#include "nxdn/nxdn.h"
#include "nxdn/nxdn_aes.h"

static void unhex(const char *s, uint8_t *out, unsigned n)
{
	for (unsigned i = 0; i < n; i++) {
		unsigned x;
		assert(sscanf(s + 2 * i, "%2x", &x) == 1);
		out[i] = (uint8_t)x;
	}
}

// What is XORed into each of the 18 IMBE frames (u0..u7 packed MSB first) of the superframe
static void p25_vectors(void)
{
	static const char *const masks[18] = {
		"f571cb029a93aeff9bdb7b", "317fb85b09f548d7ce6193", "144ce08f62a453abe5e540",
		"c10a3e2f54f7692d707ebf", "dfe31ccf53509c890c30c3", "3c9dd5d2671c98b57138bf",
		"05ae2f2250d471b0f2c337", "873be6db55a512f359c9a8", "173eaf102f8aa448c5d899",
		"b1511bb4e71228410744a3", "89da845232d10771333120", "7577848f2626a4aa396a6f",
		"b9af2090cf197ceee18570", "d381089575f6fa21aa6247", "8025feceda2a8c8f72f360",
		"2c95e8a6bc8b450969574c", "e8f7b5321e62fab980355b", "f396bcc07e65b93586017f",
	};
	uint8_t key[32], mi[8], want[11], got[11];
	aes_ofb_t ofb;

	unhex("0123456789abcdef23456789abcdef01456789abcdef01236789abcdef012345", key, 32);
	unhex("1234567890abcdef", mi, 8);
	// The MI LFSR: two steps of the same MI as OP25 makes them
	p25_aes_mi_next(mi);
	unhex("6fe2802aa403828b", want, 8);
	assert(memcmp(mi, want, 8) == 0);
	p25_aes_mi_next(mi);
	unhex("ff35b1909449b835", want, 8);
	assert(memcmp(mi, want, 8) == 0);
	unhex("1234567890abcdef", mi, 8);
	for (unsigned ldu = 0; ldu < 2; ldu++) {
		p25_aes_start(&ofb, key, mi, ldu != 0);
		for (unsigned n = 0; n < 9; n++) {
			memset(got, 0, sizeof got);
			if (n == 8)
				aes_ofb_skip(&ofb, 2);	// as p25_call.c does: the LSD's two octets
			aes_ofb_xor(&ofb, got, sizeof got);
			unhex(masks[9 * ldu + n], want, sizeof want);
			assert(memcmp(got, want, sizeof got) == 0);
		}
	}
}

static void nxdn_vectors(void)
{
	// TS 1-D 7.2.1.3, EHR: the 1031 Hz tone pattern encrypted over two superframes, one line a VCH
	static const char *const frames[NXDN_AES_SESSION_VCH] = {
		"BD4503BDC7F187AF31", "72FFC7506DB58330D0", "5C0BC6F1471DDE572B", "17D7202AF5EA342472",
		"4AEF6528DA8145E9BD", "916D09EC70C2D7E5FC", "15A25D968FD1A7F14E", "DB6E471655BBA93502",
		"6579296763DCF3F8CD", "4E97E1AB77B9C8E5C8", "A1390C285695DB3667", "BBAE0D4F69A2FC8AFF",
		"2D60701DF171C735B7", "F40E8FB1BB3F9A558D", "7485751FE484A653BB", "644E705B916A310BCF",
		"50DD37B40C5EBD3638", "6CB7C18B179DB8F2CB", "49245FCEB8D050735F", "7E2D9F89A6ED1FA25F",
		"E894C53A4D7B5AFBBF", "2CB7A7B4B827B6BD42", "D1F438BC58B930432B", "664A0B0155CBE78BBC",
		"C683C9BAE6002F0D04", "E8D8D9ACDA07ADE722", "79B5E87D9E41418C13", "E220F3E27131907C6B",
		"2948D6C1C2EFA230C0", "73B3D3BCFA3E3BB0C7", "00D78E91160FD8FA75", "0FA9A6E8D5160D1F3C",
	};
	uint8_t key[32], iv[8], next[8], want[10], got[10], tone[9];

	unhex("ABCDEF0123456789CDEF0123456789ABEF0123456789ABCD0123456789ABCDEF", key, 32);
	unhex("ABCDEF1234567890", iv, 8);
	memcpy(next, iv, 8);
	aes_voice_iv_next(next);
	unhex("8B8DDEEB890F4CF1", want, 8);
	assert(memcmp(next, want, 8) == 0);
	unhex("CEA8FE83ACC458200A", tone, 9);	// TS 1-A 7.2.1.1, with its FEC
	for (unsigned n = 0; n < NXDN_AES_SESSION_VCH; n++) {
		memcpy(got, tone, 9);
		nxdn_aes_vch(key, iv, n, got);
		unhex(frames[n], want, 9);
		if (memcmp(got, want, 9)) {
			fprintf(stderr, "NXDN standard frame %u differs\n", n);
			assert(0);
		}
		got[0] ^= 0x80;	// a channel error in C0 (air bit 0): the Golay code corrects it before decrypting
		nxdn_aes_vch(key, iv, n, got);
		assert(memcmp(got, tone, 9) == 0);
	}
	// VCALL_IV: type in octet 0, IV in 1-8, so a SACCH superframe (72 bits) carries all of it
	nxdn_viv_build(iv, got);
	unhex("03ABCDEF123456789000", want, 10);
	assert(memcmp(got, want, 10) == 0);
	memset(next, 0, sizeof next);
	assert(nxdn_viv_parse(got, next) && memcmp(next, iv, 8) == 0);
	got[0] = NXDN_MSG_VCALL;
	assert(!nxdn_viv_parse(got, next));
}

int main(void)
{
	p25_vectors();
	nxdn_vectors();
	puts("voiceCipherTest: P25 18 reference masks, NXDN 32 standard frames and VCALL_IV passed");
	return 0;
}
