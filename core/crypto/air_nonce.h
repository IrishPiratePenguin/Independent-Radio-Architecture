// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// Unique message indicators and IVs for the air modes' encryption (docs/spec/air-encryption.md 1). Output feedback mode
// leaks the plaintext if a key is ever used twice with the same IV, so an IV must never repeat. This one is built from
// a number that changes at every start (the epoch, kept in the settings and raised by one at each boot), a counter of
// the transmissions since, and the time, and is then put through AES under the key, so it also looks random
#ifndef RFOUNDRY_AIR_NONCE_H
#define RFOUNDRY_AIR_NONCE_H

#include <stdint.h>
#include "aes256.h"

// The epoch of this start (settings_t.air_epoch after its increment). Without a call it is 0
void air_nonce_set_epoch(uint16_t epoch);
// 16 octets no transmission of this start has had before, nor (while the epoch differs) any other start's
void air_nonce(const uint8_t key[AES256_KEY_BYTES], uint32_t now_ms, uint8_t out[AES_BLOCK_BYTES]);

#endif
