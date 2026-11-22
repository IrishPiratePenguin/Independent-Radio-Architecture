// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// P25 AES-256 voice keystream (p25_aes.h)
#include "../config.h"
#if CONFIG_AIR_AES	// the whole file
#include "p25_aes.h"

#include <string.h>

// The register is the indicator's 64 bits, first octet most significant. Taps: the powers of x in the polynomial, 64
// being the bit shifted out (bit 63) and the others counted from it
void p25_aes_mi_next(uint8_t mi[P25_MI_CIPHER_BYTES])
{
	aes_voice_iv_next(mi);
}

bool p25_aes_mi_valid(const uint8_t mi[P25_MI_CIPHER_BYTES])
{
	uint8_t any = 0;

	for (uint8_t i = 0; i < P25_MI_CIPHER_BYTES; i++)
		any |= mi[i];
	return any != 0;
}

void p25_aes_start(aes_ofb_t *o, const uint8_t key[AES256_KEY_BYTES], const uint8_t mi[P25_MI_CIPHER_BYTES], bool ldu2)
{
	uint8_t iv[AES_BLOCK_BYTES];

	memcpy(iv, mi, P25_MI_CIPHER_BYTES);
	memcpy(iv + P25_MI_CIPHER_BYTES, mi, P25_MI_CIPHER_BYTES);
	p25_aes_mi_next(iv + P25_MI_CIPHER_BYTES);
	aes_ofb_init(o, key, iv);
	// TIA-102.AAAD-A: discard the first block and voice interval, retaining the LSD positions.
	aes_ofb_skip(o, AES_BLOCK_BYTES + 11u + (ldu2 ? P25_AES_LDU_BYTES + 2u : 0u));
}

#endif
