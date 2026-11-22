// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// NXDN AES-256 voice keystream (nxdn_aes.h)
#include "../config.h"
#if CONFIG_AIR_AES	// the whole file
#include "nxdn_aes.h"
#include "nxdn.h"
#include "dmr/ambe/ambeFec.h"

#include <string.h>

void nxdn_aes_vch(const uint8_t key[AES256_KEY_BYTES], const uint8_t iv[NXDN_IV_BYTES], uint32_t n,
		  uint8_t vch[NXDN_VCH_BYTES])
{
	uint8_t block[AES_BLOCK_BYTES], bits[AMBE_PARAM_BITS];
	uint32_t first = n * AMBE_PARAM_BITS, done = 0;

	if (n >= NXDN_AES_SESSION_VCH)
		return;
	// The 128-bit IV: the seed, then the LFSR after 64 shifts (TS 1-D 5.3.3.2). The first output block only
	// re-initializes the input block and is never applied (TS 1-D 5.3.2.1 e-v, kept for AES by 5.3.3.1)
	memcpy(block, iv, NXDN_IV_BYTES);
	memcpy(block + NXDN_IV_BYTES, iv, NXDN_IV_BYTES);
	aes_voice_iv_next(block + NXDN_IV_BYTES);
	aes256_encrypt_block(key, block, block);
	// ponytail: the session's blocks are regenerated for each VCH (at most 13 AES per 20 ms), cache them if CPU matters
	ambeFecDecode(vch, bits, NULL);
	for (unsigned i = 0; i < AMBE_PARAM_BITS; i++) {
		uint32_t at = first + i;

		while (done <= at / 128u) {
			aes256_encrypt_block(key, block, block);
			done++;
		}
		bits[i] ^= (block[(at % 128u) / 8u] >> (7u - at % 8u)) & 1u;
	}
	ambeFecEncode(bits, vch);
}

void nxdn_viv_build(const uint8_t iv[NXDN_IV_BYTES], uint8_t out[NXDN_VIV_BYTES])
{
	memset(out, 0, NXDN_VIV_BYTES);
	out[0] = NXDN_MSG_VCALL_IV;
	memcpy(&out[1], iv, NXDN_IV_BYTES);
}

bool nxdn_viv_parse(const uint8_t *msg, uint8_t iv[NXDN_IV_BYTES])
{
	if (nxdn_msg_type(msg) != NXDN_MSG_VCALL_IV)
		return false;
	memcpy(iv, &msg[1], NXDN_IV_BYTES);
	return true;
}

#endif
