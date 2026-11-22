// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// NXDN voice encryption with AES-256 (CONFIG_AIR_AES; docs/spec/air-encryption.md 3), as NXDN TS 1-D 5.3.3 and
// 5.4.4.2 define it: AES-256 OFB over the 49 voice bits of each VCH, before their FEC, and the VCALL_IV that carries
// the IV (TS 1-A 6.4.1.2). Checked against the standard's own test vectors (TS 1-D 7.2.1.3, tests/host/voiceCipherTest.c).
#ifndef RFOUNDRY_NXDN_AES_H
#define RFOUNDRY_NXDN_AES_H

#include <stdbool.h>
#include <stdint.h>
#include "crypto/aes256.h"

#define NXDN_IV_BYTES		8
#define NXDN_VCH_BYTES		9	// a voice channel: 72 bits
#define NXDN_VIV_BYTES		10	// the VCALL_IV message as a FACCH1 carries it (9 octets, then one of padding)
#define NXDN_AES_SESSION_VCH	32	// an encryption session: two superframes, four VCH positions a frame

// Voice channel n (0..NXDN_AES_SESSION_VCH-1) of the encryption session whose IV (seed) is iv. A frame that carries
// FACCH1 keeps its four positions. The frame's FEC is corrected, the 49 voice bits XORed, the FEC made again: the
// same call encrypts and decrypts
void nxdn_aes_vch(const uint8_t key[AES256_KEY_BYTES], const uint8_t iv[NXDN_IV_BYTES], uint32_t n,
		  uint8_t vch[NXDN_VCH_BYTES]);
// The VCALL_IV message: type 0x03 (F1, F2 spare), then the IV in octets 1-8. Its first 72 bits fit a superframe's SACCH
void nxdn_viv_build(const uint8_t iv[NXDN_IV_BYTES], uint8_t out[NXDN_VIV_BYTES]);
// False when msg is not a VCALL_IV. Reads octets 0-8 only
bool nxdn_viv_parse(const uint8_t *msg, uint8_t iv[NXDN_IV_BYTES]);

#endif
