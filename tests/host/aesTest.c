// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// AES-256 and output feedback mode (core/crypto/aes256.c) against the standard's own vectors: FIPS 197 appendix C.3 and
// NIST SP 800-38A F.4.5 (OFB-AES256, four blocks)
#include <stdio.h>
#include <string.h>
#include "crypto/aes256.h"

static int checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void hex(const char *s, uint8_t *out, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		unsigned v;
		sscanf(s + 2 * i, "%2x", &v);
		out[i] = (uint8_t)v;
	}
}

static void test_fips197(void)
{
	uint8_t key[32], in[16], want[16], out[16];

	// C.3: key 000102...1f, plaintext 00112233445566778899aabbccddeeff
	for (int i = 0; i < 32; i++)
		key[i] = (uint8_t)i;
	hex("00112233445566778899aabbccddeeff", in, 16);
	hex("8ea2b7ca516745bfeafc49904b496089", want, 16);
	aes256_encrypt_block(key, in, out);
	CHECK(memcmp(out, want, 16) == 0);
	aes256_encrypt_block(key, in, in);	// in place
	CHECK(memcmp(in, want, 16) == 0);
}

static void test_ofb(void)
{
	uint8_t key[32], iv[16], buf[64], want[64];

	hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", key, 32);
	hex("000102030405060708090a0b0c0d0e0f", iv, 16);
	hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
	    "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710", buf, 64);
	hex("dc7e84bfda79164b7ecd8486985d3860"
	    "4febdc6740d20b3ac88f6ad82a4fb08d"
	    "71ab47a086e86eedf39d1c5bba97c408"
	    "0126141d67f37be8538f5a8be740e484", want, 64);
	aes_ofb_t o;
	aes_ofb_init(&o, key, iv);
	aes_ofb_xor(&o, buf, 64);
	CHECK(memcmp(buf, want, 64) == 0);
	// Decrypting is the same operation, and the stream may be taken in odd pieces
	aes_ofb_init(&o, key, iv);
	aes_ofb_xor(&o, buf, 5);
	aes_ofb_xor(&o, buf + 5, 27);
	aes_ofb_xor(&o, buf + 32, 32);
	uint8_t plain[64];
	hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
	    "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710", plain, 64);
	CHECK(memcmp(buf, plain, 64) == 0);
	// Skipping octets lands on the same keystream
	aes_ofb_init(&o, key, iv);
	aes_ofb_skip(&o, 21);
	uint8_t z[8] = { 0 };
	aes_ofb_xor(&o, z, 8);
	uint8_t ks[64];
	for (int i = 0; i < 64; i++)
		ks[i] = (uint8_t)(want[i] ^ plain[i]);
	CHECK(memcmp(z, ks + 21, 8) == 0);
	aes_ofb_wipe(&o);
	CHECK(o.key[0] == 0 && o.reg[0] == 0);
}

int main(void)
{
	test_fips197();
	test_ofb();
	printf("# aesTest: %d checks, %s\n", checks, failures ? "FAILED" : "PASSED");
	return failures != 0;
}
