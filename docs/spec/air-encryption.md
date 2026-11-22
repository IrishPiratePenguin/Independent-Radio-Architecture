<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (c) 2026 lyra XE1POC -->

# AES-256 voice encryption of the air modes (`CONFIG_AIR_AES`)

P25 and NXDN conventional channels can send their voice encrypted with AES-256 and play calls encrypted with the same
key; DMR does too, as a privacy key type (8). The cipher is in `core/crypto` (FIPS 197, checked against its vectors), the P25 part in `core/p25/p25_aes.c`
and `p25_call.c`, the NXDN part in `core/nxdn/nxdn_aes.c` and `nxdn_call.c`. Where the law forbids encryption (amateur
frequencies in most countries) the owner leaves a channel's key off: nothing here is on until a channel says so.

**Status.** Both keystreams follow the standards' layout. NXDN matches the 32 encrypted voice channels of the
standard's own test vector (TS 1-D 7.2.1.3); P25 matches the masks OP25 computes for one key and indicator
(TIA-102.AAAD-A layout). Tested between two instances of this firmware (host tests, two engines and a radio core, both
modes). Not checked against another maker's radio on air: section 6 lists what that leaves open.

## 1. Keys, indicators and the settings

| Item | Rule |
|---|---|
| Key | 32 octets. Kept in the keyring (4); a channel names one by key ID |
| Channel | Its parameter block says AES on or off and the key ID (2.4, 3.4). Off: the channel is clear, as ever |
| No key | A channel with AES on whose key is not in the keyring plays nothing encrypted and **refuses PTT**: it never sends in the clear instead |
| Unique IV | Output feedback leaks the plaintext if a key is used twice with the same IV. Every transmission takes a fresh one from `air_nonce()`: AES-256 under the key of `epoch (2) / transmission count (4) / time in ms (4) / zeros`. The epoch is `settings_t.air_epoch`, raised by one at each boot, so no start repeats another's; the count restarts at 0 at each boot |
| Secrets | The keyring uses traffic-key records ([channel-modes.md](channel-modes.md) 5.4). Codeplug exports and zone shares omit key material. |

## 2. P25 (`CODEPLUG_KIND_P25`)

### 2.1 On air

| Field | Value |
|---|---|
| Algorithm id | `0x84` (AES-256), in the header (HDU) and in every LDU2's encryption sync |
| Key id | 16 bits, in the same places |
| Message indicator | 72 bits on air: the first 64 are the cipher's, the last 8 are sent as 0 (and ignored on receive). The HDU carries the first superframe's; each LDU2 carries the **next** superframe's |
| Next indicator | The LFSR x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1 shifted 64 times: what an LDU2 sends, and what a receiver steps to when an LDU2's encryption sync is lost |

### 2.2 The keystream

Output feedback (SP 800-38A 6.4), key = the channel's, 128-bit IV = the 64-bit indicator followed by the indicator
stepped once (2.1). The first 16 octets of output are discarded. Then, per superframe:

| Stream octets (after the discarded 16) | Use |
|---|---|
| 0-10 | Not applied (the interval before the first voice frame) |
| 11-98 | LDU1 voice frames 1-8, 11 octets each |
| 99-100 | LDU1 low-speed data (sent clear here: the octets are skipped) |
| 101-111 | LDU1 voice frame 9 |
| 112-212 | LDU2 the same way: frames 1-8, two LSD octets, frame 9 |

The 88 bits of a frame (u0-u7 packed first bit first) are XORed **before their FEC** (the IMBE frame is encoded after,
as for clear voice), so encrypted voice has the frame structure, status symbols and error correction of clear voice.
Silence frames sent to fill an LDU are encrypted too.

### 2.3 Receive

| Case | What happens |
|---|---|
| AES-256, the key id of the channel's key, the indicator known | Played, decrypted. The call's fields show as for any call; the call line says `AES` |
| Another key id, another algorithm, or no key | The call is shown, not played: "Encrypted: not played" |
| A late entry | The first LDU2's encryption sync gives the next superframe's indicator: voice plays from the following LDU1. Each LDU2 resynchronizes |
| Wrong key | Noise: the engine cannot tell (there is no check value) |

### 2.4 The channel block

Bytes 5-7 of the P25 conventional parameter block ([p25-codeplug.md](p25-codeplug.md) 2): byte 5 bit 0 AES-256 on, bytes
6-7 the key ID (little endian). Blocks without them (older codeplugs) are clear.

## 3. NXDN (`CODEPLUG_KIND_NXDN`)

### 3.1 On air

| Field | Value |
|---|---|
| Cipher type | 3 (AES), in the VCALL |
| Key id | 6 bits (0-63), in the VCALL |
| IV | 64 bits, in a VCALL_IV (TS 1-A 6.4.1.2: octet 0 type `0x03`, octets 1-8 the IV; 72 bits, so it fits a SACCH superframe) |
| Where the IV goes | The header frame's second FACCH1: the first session's IV. Then the SACCH alternates: the VCALL in a session's first superframe, a VCALL_IV with the **next** session's IV in its second (TS 1-D 5.4.4.2 (4)). At 9600 bit/s the frames that carry FACCH1 send the VCALL and the next session's VCALL_IV |

### 3.2 The keystream

TS 1-D 5.3.3 and 5.4.4.2. An **encryption session** is two superframes (8 frames, 32 VCH positions). Its 128-bit IV
is the session's 64-bit IV (the LFSR seed) followed by the LFSR (2.1's polynomial) shifted 64 times; the next session's
IV is that shifted LFSR. Output feedback, key = the channel's; the first output block is discarded (TS 1-D 5.3.2.1 e-v,
which 5.3.3.1 keeps for AES). The stream's bits then go 49 to a VCH position, position n = 4 x frame + VCH, 1568 bits
(12.25 blocks) a session. The 49 voice bits of a VCH are XORed **before their FEC**: the receiver corrects the frame
(Golay) first, decrypts, and encodes the FEC again, so a channel bit error stays one bit. A frame that carries FACCH1
(9600 bit/s) leaves its four positions unused.

### 3.3 Receive

Played, decrypted, when the call is AES with the channel's key id and the session is known: from the header frame's
IV, or, for a receiver that joins mid-call, from the first whole VCALL_IV in a SACCH (it ends a session, so the next
frame starts the session it names: up to two sessions, 1.3 s, are lost). Each later VCALL_IV resynchronizes; a lost
one is replaced by stepping the LFSR. Another key id or no key: shown as encrypted, not played.

### 3.4 The channel block

Bytes 3-4 of the NXDN conventional parameter block ([nxdn-codeplug.md](nxdn-codeplug.md) 2): byte 3 bit 0 AES-256 on,
byte 4 the key ID (0-63).

## 4. The keyring

Traffic key records ([channel-modes.md](channel-modes.md) 5.4) with **system 0xFF** (none): the radio's own keyring.

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | System: 0xFF |
| 1 | 1 | Algorithm: 0x84 for P25, 3 for NXDN, 0x25 for DMR AES, 0x21 for DMR Enhanced Privacy |
| 2 | 2 | Key ID, little endian |
| 4 | 1 | Flags (0) |
| 5 | 32 or 5 | AES-256 or ARC4-40 key |

In the Codeplug Editor, use **Radio → Keys** to enter the algorithm, key ID and key. AES-256 accepts 64 hexadecimal
digits and DMR Enhanced Privacy accepts 10. The input masks the key. The radio reads the matching algorithm, ID and
key length when the channel is tuned.

## 5. What the radio shows

| Where | What |
|---|---|
| Call line | `AES ID 1234 > TG 1` for an encrypted call that plays; "Encrypted: not played" for one that does not |
| PTT | Refused ("refused") for a channel with AES on and no key in the keyring |
| Memory | 32 octets of key and a few of state while an air mode runs, in the memory it already uses (P25: the lent vocoder block); NXDN: 32 octets in its engine |

## 6. Not checked on air, not verified against others

- **Interoperability.** NXDN matches the standard's test vector bit for bit; P25 matches OP25's reading of
  TIA-102.AAAD-A (not the TIA document's own vectors, which are not public). Neither has met another maker's radio on
  air. What a radio may still do differently: the content of the FACCH1 frames at 9600 bit/s (the standard leaves it
  open), NXDN key IDs above 63, a P25 radio that sends the LSD encrypted.
- **NXDN position by counting.** The receiver counts frames from the session start. A frame lost without the framer
  placing it shifts the count until the next VCALL_IV.
- **Randomness.** Indicators are unique, not secret and not random; their purpose is to never repeat.
- **A key on a radio** is as safe as the radio: the codeplug holds it in flash.
- The voice quality of an encrypted call, the effect on range and sensitivity, and a live system are untested.

## 7. Tests

| Test | What |
|---|---|
| `aesTest` | AES-256 against FIPS 197 C.3 and OFB-AES256 against SP 800-38A F.4.5 |
| `voiceCipherTest` | P25: the MI LFSR and the 18 frame masks of a superframe against OP25's. NXDN: the 32 VCH of TS 1-D 7.2.1.3 from the TS 1-A tone pattern, the IV LFSR, a corrected channel error, the VCALL_IV layout |
| `p25EngineTestAes` | An encrypted call over many superframes in order; a wrong key; another key id; no key; PTT refused without a key; a late entry |
| `dmrAesTest` | DMR: the PI header, three superframes of a real AES-256 recording with a published key (only the keystream each frame carries is kept), a superframe lost and told by the slot counter, TX to RX, late entry (the next MI in the AMBE bits, Golay-corrected) |
| `dmrCallTestAes` | DMR through the call stack: the PI header after the voice LC headers, encrypted bursts the key decrypts, a receiver with the key ID plays them, another key ID or no key never does, the key ID without its key refuses PTT, late entry (burst F's single block and the superframe's MI fragments start a receiver that never heard the PI header; another key ID or no single block stays silent) |
| `nxdnEngineTestAes` | The same at 4800 and 9600 bit/s; the late entry decrypts from the next session, every frame right |
| `radioCoreTestP25`, `radioCoreTestNxdn` | A stored channel with AES and a keyring: plays, another key id is silent, PTT sends encrypted (a far radio with the key hears it, one without does not), a key missing from the keyring refuses PTT |
| `p25CodeplugTest`, `nxdnCodeplugTest`, `site/tests/modes.test.mjs` | The channel block bytes (shared vectors) and the keyring records |

## 8. DMR privacy

In the conventional DMR channel editor, choose **Privacy** and then **Key ID**. **Radio's** inherits the radio-wide
privacy setting, **Off** sends clear, **Moto Basic** accepts key numbers 1–255, and **Moto Enhanced** and **AES-256**
use IDs 1–255 from their respective CPS keyrings. Digits can be typed directly for Basic Privacy. Selecting a private
method with a missing key blocks PTT. Digital Monitor widens receive filtering without changing the transmit profile.

Named memories use the existing three-byte extension at `0x34`. The little-endian markers `0x5AA5`, `0x5AA6`,
`0x5AA7` and `0x5AA8` identify AES, Basic, Enhanced and Off, followed by the one-byte key number or ID. The absent
marker preserves the radio-wide setting for existing channels. Normal CPS exports leave the selected method and
keyring references intact and omit the Basic key number and raw traffic keys.

### Basic Privacy and RAS

Motorola Basic Privacy uses the existing AMBE+2 transform. Key 179 uses table seed `0xDCB9`. Digital Monitor
can acquire six sequential voice bursts when RAS invalidates the LC. A valid EMB privacy bit stays effective
through the rest of that call, including burst A, which carries voice sync in place of EMB. The configured Motorola
Basic key also applies to an unidentified call on the configured color code and slot when the repeater omits the EMB
privacy bit. Without signalling an unidentified clear call on the same color code and slot cannot be distinguished
from Basic Privacy. A burst from another slot or color code never contributes to an active call's privacy state.

### AES-256 and Enhanced Privacy

Both use the Motorola PI header and late-entry framing below. Enhanced Privacy uses algorithm `0x21` or compatible
`0x01` on receive. ARC4 uses five key bytes followed by the four big-endian MI bytes, discards 256 output bytes,
and consumes seven bytes per frame. The next MI advances 32 times with feedback `bit31 XOR bit3 XOR bit1`. The two
algorithms share the burst-position and late-entry checks and use separate keyring entries.

| Item | Rule |
|---|---|
| PI header | Data type 0, once after the voice LC headers: algorithm 0x25 (0x05 is taken too), FID 0x10, key ID, 32-bit MI, destination, CRC-CCITT masked 0x6969 |
| Voice LC | FID 0x10, the privacy service option, as for basic privacy |
| IV | The MI, then 96 more bits of its LFSR x^32 + x^22 + x^2 + x + 1 |
| Keystream | AES-256 OFB, the first block discarded; 126 octets a superframe, 7 for each of its 18 frames, the first 49 bits XORed onto the frame's voice bits before their FEC (RX corrects the frame first) |
| Next superframe | The MI shifted 32 times through the same LFSR |
| TX MI | From `air_nonce()` (1), never 0 |
| Single block (burst F) | The key ID in bits 10-3 and the algorithm's low 3 in bits 2-0, with single-burst BPTC (B.2.2.2). A matching algorithm and key ID takes priority over an ambiguous reverse-channel decode during receive. |
| Late-entry MI | Each superframe's AMBE frames carry the NEXT superframe's MI in their least significant bits (four per frame): three Golay(24,12) words hold the 32-bit MI and a 4-bit CRC (x^4 + x + 1, inverted). Bursts A-C carry the words' first halves, D-F the second halves |
| RX | A PI header with our key ID starts the keystream; another key ID or algorithm stops it, and the call is not played. Superframes lost are counted by the modem's slot counter (up to 64); a call back after a fade keeps its keystream. A frame that is silence on air, or whose C0 word fails, plays as silence. Late entry: a call joined after its PI header starts from burst F's single block (our key ID, AES-256) and the superframe's MI fragments, from the next superframe on; the superframe the fragments came in stays silent. A single block with another key ID or algorithm, or none, leaves the call unplayed |
| No key | The key ID set and no keyring key for it: calls with that key ID are not played and PTT is refused |

Checked against a public recording (the archive.org item dmr-encrypted-samples_202511, its published key, as dmrgo's
tests carry it): 180 of 180 frames over 10 superframes. Not tried against another radio on air.
