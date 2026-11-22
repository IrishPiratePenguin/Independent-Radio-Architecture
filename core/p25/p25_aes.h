// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// P25 voice encryption with AES-256 (CONFIG_AIR_AES; docs/spec/air-encryption.md 2): the algorithm id, how a message
// indicator becomes a keystream, and how the indicator advances. The keystream XORs the 88-bit voice frames before
// their FEC, so it rides the same header (HDU) and encryption sync (LDU2) every P25 radio sends
#ifndef RFOUNDRY_P25_AES_H
#define RFOUNDRY_P25_AES_H

#include <stdbool.h>
#include <stdint.h>
#include "crypto/aes256.h"

#define P25_ALGID_AES256	0x84	// TIA-102 algorithm id of AES-256
#define P25_MI_BYTES		9	// the message indicator on air: 72 bits. The cipher uses the first 8 and sends the last as 0
#define P25_MI_CIPHER_BYTES	8
#define P25_AES_LDU_BYTES	99	// the keystream one LDU uses: nine 11-octet voice frames

// The indicator of the next voice superframe (an LDU1 and its LDU2): the 64-bit LFSR x^64 + x^62 + x^46 + x^38 + x^27 +
// x^15 + 1 run for 64 shifts. An all-zero indicator maps to itself and is never valid
void p25_aes_mi_next(uint8_t mi[P25_MI_CIPHER_BYTES]);
bool p25_aes_mi_valid(const uint8_t mi[P25_MI_CIPHER_BYTES]);
// TIA-102.AAAD-A OFB from mi || next(mi), after the discarded block/voice interval. Each LDU skips two LSD
// octets before its ninth voice frame; ldu2 starts after the first LDU and its LSD interval.
void p25_aes_start(aes_ofb_t *o, const uint8_t key[AES256_KEY_BYTES], const uint8_t mi[P25_MI_CIPHER_BYTES], bool ldu2);

#endif
