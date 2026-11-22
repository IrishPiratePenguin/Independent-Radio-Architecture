// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// Unique indicators for the air modes' encryption (air_nonce.h)
#include "air_nonce.h"

#include <string.h>

static uint16_t epoch;
static uint32_t count;

void air_nonce_set_epoch(uint16_t e)
{
	epoch = e;
	count = 0;
}

void air_nonce(const uint8_t key[AES256_KEY_BYTES], uint32_t now_ms, uint8_t out[AES_BLOCK_BYTES])
{
	uint8_t in[AES_BLOCK_BYTES] = { 0 };
	uint32_t n = count++;

	in[0] = (uint8_t)(epoch >> 8);
	in[1] = (uint8_t)epoch;
	in[2] = (uint8_t)(n >> 24);
	in[3] = (uint8_t)(n >> 16);
	in[4] = (uint8_t)(n >> 8);
	in[5] = (uint8_t)n;
	in[6] = (uint8_t)(now_ms >> 24);
	in[7] = (uint8_t)(now_ms >> 16);
	in[8] = (uint8_t)(now_ms >> 8);
	in[9] = (uint8_t)now_ms;
	aes256_encrypt_block(key, in, out);
}
