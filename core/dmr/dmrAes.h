// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// DMR AES-256 voice in the Motorola layout (CONFIG_AIR_AES with CONFIG_DMR_PRIVACY; docs/spec/air-encryption.md 8).
// A PI header (FID 0x10) carries algorithm 0x25, an 8-bit key ID and a 32-bit MI. The MI and the LFSR
// x^32 + x^22 + x^2 + x + 1 give the 128-bit IV; AES-256 OFB, first block discarded, gives 126 octets a superframe:
// 7 for each of its 18 frames, the first 49 bits XORed onto the frame's voice bits before their FEC. Each superframe
// steps the MI 32 times. The IV expansion and keystream order follow dmrgo (github.com/USA-RedDragon/dmrgo, MIT,
// Copyright 2023-2026 Jacob McSwain); checked against a real recording with a published key (tests/host/dmrAesTest.c).
#ifndef RFOUNDRY_DMRAES_H
#define RFOUNDRY_DMRAES_H

#include <stdbool.h>
#include <stdint.h>

#define DMR_AES_ALG		0x25	// AES-256 in the PI header (0x05 is taken too: what some makers send)
#define DMR_ARC4_ALG		0x21	// Motorola Enhanced Privacy (also 0x01 in a compatible PI)
#define DMR_ARC4_KEY_BYTES	5
#define DMR_AES_FID		0x10
#define DMR_AES_PI_BYTES	10	// the PI header without its CRC
#define DMR_AES_SF_BYTES	126	// a superframe's keystream: 18 frames of 7 octets
#define DMR_AES_MAX_GAP		64	// superframes a call may lose and keep its keystream (23 s; a fade ends it sooner)

typedef struct {
	const uint8_t *key;		// caller's 32 AES octets or 5 ARC4 octets
	uint32_t mi;			// the MI of the superframe in hand
	uint32_t slot;			// the modem's slot counter at the last burst, 0: none
	uint8_t pos;			// the last burst's index (0-5), 0xFF: none yet
	bool ready;
	uint8_t alg;
	uint8_t ks[DMR_AES_SF_BYTES];
} dmr_aes_t;

uint32_t dmrAesMiNext(uint32_t mi);
void dmrAesStart(dmr_aes_t *a, const uint8_t *key, uint32_t mi);
void dmrVoiceCipherStart(dmr_aes_t *a, uint8_t alg, const uint8_t *key, uint32_t mi);
uint32_t dmrVoiceCipherNextMi(uint8_t alg, uint32_t mi);
bool dmrVoiceCipherPiParse(const uint8_t pi[DMR_AES_PI_BYTES], uint8_t alg, uint8_t *kid, uint32_t *mi);
void dmrVoiceCipherPiBuild(uint8_t alg, uint8_t kid, uint32_t mi, uint32_t dst, uint8_t pi[DMR_AES_PI_BYTES]);
void dmrAesStop(dmr_aes_t *a);	// wipes the keystream
// The first 10 octets of a PI header: true for AES-256 in the Motorola layout, with a key ID and an MI
bool dmrAesPiParse(const uint8_t pi[DMR_AES_PI_BYTES], uint8_t *kid, uint32_t *mi);
void dmrAesPiBuild(uint8_t kid, uint32_t mi, uint32_t dst, uint8_t pi[DMR_AES_PI_BYTES]);
// Encrypt or decrypt voice burst index (0 = A .. 5 = F) in place: its 3 frames of 9 octets. The MI steps for each
// superframe since the last burst (slot: the modem's slot counter, 0 without one: the burst index alone tells). RX
// passes the silence frame: a frame that is silence on air, or whose first FEC word fails, becomes it. False: no
// keystream, or more than DMR_AES_MAX_GAP superframes lost (the stream is then stopped)
bool dmrAesBurst(dmr_aes_t *a, uint8_t ambe27[27], uint8_t index, uint32_t slot, const uint8_t *silence);
// Late entry (ETSI TS 102 361-1 9.3.3): the next superframe's MI, carried in the AMBE frames' least significant
// bits, and the key ID / algorithm in burst F's single burst. frag[burst][codeword], burst 0 = A .. 5 = F,
// codeword 0..2: the three Golay(24,12) words holding the 32-bit MI and its 4-bit CRC
void dmrAesLeBuild(uint32_t mi, uint8_t frag[6][3]);
// The MI in a superframe's fragments, or false when a Golay word is uncorrectable or the CRC fails
bool dmrAesLeDecode(const uint8_t frag[6][3], uint32_t *mi);
// Write this burst's three nibbles into its frames (after encryption: the bits are sent in the clear), or read
// them from a received burst (before decryption)
void dmrAesLeStuff(uint8_t v[27], uint8_t index, const uint8_t frag[6][3]);
void dmrAesLeCollect(const uint8_t v[27], uint8_t index, uint8_t frag[6][3]);

#endif
