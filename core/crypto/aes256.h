// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// AES-256 (FIPS 197) for the air modes' voice encryption (docs/spec/air-encryption.md): the block cipher in the encrypt
// direction only, which is all output feedback mode (NIST SP 800-38A 6.4) needs. The key schedule runs a round at a
// time on a 32-octet window, so a cipher costs no RAM beyond the key and the 16-octet feedback register.
// Not constant time (the S-box is a table): the radio has no attacker who can time it
#ifndef RFOUNDRY_AES256_H
#define RFOUNDRY_AES256_H

#include <stddef.h>
#include <stdint.h>

#define AES256_KEY_BYTES	32u
#define AES_BLOCK_BYTES		16u

// P25/NXDN 64-bit voice indicator LFSR, 64 shifts, most significant octet first.
void aes_voice_iv_next(uint8_t mi[8]);

// out = AES-256(key, in); in and out may be the same
void aes256_encrypt_block(const uint8_t key[AES256_KEY_BYTES], const uint8_t in[AES_BLOCK_BYTES],
			  uint8_t out[AES_BLOCK_BYTES]);

// Output feedback mode: the keystream is AES(key, register) fed back, from the IV
typedef struct {
	uint8_t key[AES256_KEY_BYTES];
	uint8_t reg[AES_BLOCK_BYTES];	// the keystream block in use
	uint8_t pos;			// octets of it used; AES_BLOCK_BYTES = a new block is due
} aes_ofb_t;

// Start a keystream. The first octet comes from AES(key, iv)
void aes_ofb_init(aes_ofb_t *o, const uint8_t key[AES256_KEY_BYTES], const uint8_t iv[AES_BLOCK_BYTES]);
// XOR the next n octets of the keystream into buf (encrypt and decrypt are the same)
void aes_ofb_xor(aes_ofb_t *o, uint8_t *buf, size_t n);
// Skip n octets of the keystream
void aes_ofb_skip(aes_ofb_t *o, size_t n);
// Forget the key and the keystream
void aes_ofb_wipe(aes_ofb_t *o);

#endif
