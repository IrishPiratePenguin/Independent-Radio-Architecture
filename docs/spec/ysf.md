<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Copyright (c) 2026 lyra XE1POC (changes) -->

# YSF (Yaesu System Fusion) / C4FM

YSF conventional voice and data (Yaesu "Amateur Radio Digital Standards",
Rev. 1.02), on the air interface ([air-interface.md](air-interface.md)).
Switch: `CONFIG_YSF`, which defaults to `CONFIG_DMR` and also builds the air
interface ([build-config.md](../build-config.md)). Which radios could carry
it: [feature-matrix.md](../feature-matrix.md). Clause numbers below are the
Yaesu standard's unless they say otherwise.

State:

| Part | State |
|---|---|
| Frame codec: sync, FICH (Golay + Viterbi + deinterleave + CRC-16), DCH (deinterleave + Viterbi + whitening + CRC-16), whitening PN(9,5) | written, host tested |
| Receive, the call layer, the engine | written, host tested; RX through the generic soft modem and framer, TX through the provider's native 4FSK slot engine |
| Voice half-rate (V/D-1 and DN) | written, host tested in loopback; the 49 parameter bits go through the DMR side's voice path to the vocoder the board hosts (`hal_vocoder_t`) |
| Voice full-rate (VW, DT 3, 88 parameter bits) | RX and TX use the shared IMBE voice path on a channel set to VW |
| DG-ID squelch | written, host tested; first cut, see below |
| The radio core: mode, settings, PTT, debug link, home screen | not written: no channel mode, no codeplug kind, no editor row, no UI |
| Codeplug channels, the site's Codeplug Editor | not written |
| On air | not checked: no YSF transmission has been received or sent on a radio yet; the RX tap response (M2) is unmeasured |

## 1. On air

| Item | Value | Clause |
|---|---|---|
| Bit rate | 9600 bit/s | 4.2.1 |
| Symbol rate | 4800 symbols/s | 4.2.1 |
| Modulation | 4FSK (C4FM), 2 bits per symbol | 4.2.1 |
| Deviation of the outer symbols (+3, -3) | 2700 Hz wide, 1350 Hz narrow | 4.2.1 |
| Transmit filter | root raised cosine, roll-off 0.2 | 4.2.1 |
| Channel bandwidth | 12.5 kHz | 4.2.1 |
| Frame | 480 symbols, 100 ms | 4.2.1 |
| Frame sync | `D471C9634D`, 20 symbols (40 bits) | 4.2.1(3) |
| Sync repetition | every frame (480 symbols) | 4.2.1 |
| Call end (N2) | 4 lost frames end a call | 4.2.1(6) |

The dibit map is the air interface's (`01` +3, `00` +1, `10` -1, `11` -3).
A frame is 120 bytes (960 bits, 480 dibits):

| Bytes | Field |
|---|---|
| 0-4 | Frame sync `D471C9634D` |
| 5-29 | FICH, 25 bytes (200 bits) |
| 30-119 | Payload, 90 bytes (5 blocks of 18 bytes) |

The framer's symbols are packed four to a byte, the first on air in bits 7-6
of byte 0: exactly the frame's own packing, so the receive layer takes the
120 frame bytes as the packed symbols, with no dibit conversion.

## 2. FICH

The 32 info bits, MSB first: FI(2) CS(2) CM(2) BN(2) BT(2) FN(3) FT(3)
rsv(1) Dev(1) MR(3) VoIP(1) DT(2) SQL(1) SC(7). Dev: 0 Wide, 1 Narrow.
SC is the DG-ID: 0 none, 1..126 codes, 127 Break Out. This layer follows the
spec (MR 3 bits, CS and SQL present); MMDVM reads MR as 2 bits and exposes
neither CS nor SQL.

| Field | Values |
|---|---|
| FI | 0 header, 1 communication, 2 terminator, 3 test |
| DT | 0 V/D mode 1, 1 Data FR, 2 V/D mode 2 (DN), 3 Voice FR (VW) |
| CM | 0 group, 1 radio ID, 3 individual |
| MR | 0 direct, 1 downlink free, 2 downlink busy |

The FICH is Golay(24,12) protected, convolutionally coded R=1/2 K=5
(G1 = 1+x^3+x^4, G2 = 1+x+x^2+x^4), interleaved, with a CRC-16. The
Golay decoder corrects up to 3 errors in the 24-bit word, counting the
extension (parity) bit: an odd-parity corrected word is still valid when the
23-bit error pattern has weight 2 or less (the parity bit is the third
error); weight 3 with odd parity is four errors and is rejected.

## 3. DCH and whitening

| Unit | Data | Transmitted |
|---|---|---|
| DCH1 (V/D-1, Data FR, header, terminator) | 20 bytes | 45 bytes (360 bits) |
| DCH2 (DN) | 10 bytes | 25 bytes (200 bits) |

A header or terminator always carries two DCH1s (CSD1: destination, source,
downlink, uplink; CSD2: downlink, uplink, remote 1..4). A V/D-1 or DN
communication frame carries one DCH; a Data FR communication frame carries
two DCH1s. The DCH is deinterleaved, Viterbi decoded, de-whitened, and
CRC-16 checked; the CRC covers the whitened data.

Whitening is PN(9,5), a 9-bit register with initial value `0x1C9`, reset per
unit (4.5(2)(b)). It generates the 20 bytes MMDVM carries as the fixed
`WHITENING_DATA` table; the host test compares the two byte by byte.

## 4. Voice, half-rate

Both half-rate modes carry 49 AMBE+2 parameter bits per 20 ms block, the
same count as the DMR side's `hal_vocoder_t` interface, and the blocks go
through that interface. Whether the 49 YSF parameters are semantically the
same ones the DVSI blob expects is **not proven**: no public YSF voice
vector exists, and no real YSF voice capture has been decoded yet. Voice
would decode as garbage if they differ; that is settled only by a real
capture.

V/D-1 (DT 0): the 72-bit voice channel (VCH) is exactly the DMR AMBE+2
3600x2450 frame (Golay 24,12 + Golay 23,12 + 25 unprotected bits), so the
existing `ambeFec` encode/decode is reused directly, with no permutation and
no whitening. A failed frame follows the firmware's DMR behavior
(pass-through with concealment by repetition in the voice sink), not MMDVM's
silence substitution.

DN (DT 2): the 104-bit voice field (VCH 72 + VeCH 32, block bits 40..143)
carries the 49 parameters raw, with no Golay: bits 0..80 are parameters
`p[0..26]` tripled (three copies of each), bits 81..102 are `p[27..48]`
direct, bit 103 is zero padding on transmit and ignored on receive. The
field is whitened with the first 13 bytes of `WHITENING_DATA`, reset per
20 ms block, and interleaved 26x4 (bits, not dibits). Receive takes a
majority vote per triplet; a non-unanimous triplet counts one error. The
"triplet k = parameter k" order is inferred from Yaesu figure 4-32, not
directly observed in MMDVM (which never extracts DN parameters).

VW (DT 3) uses 88 speech bits per 20 ms block. It shares the Golay/Hamming
codes and whitening with P25 and uses the YSF-specific 144-bit interleaver.
The initial communication frame (FN=0, FT=1) carries CSD3, 72 reserved bits,
and two voice blocks. Normal communication frames carry five voice blocks.

Set the channel's YSF data type to VW to use full-rate receive and transmit.
On the DM-32UV the IMBE path borrows the AMBE vocoder's RAM for the duration
of the channel session. Select DN or V/D-1 for half-rate audio.

## 5. The engine

`core/ysf/ysf_engine.c` mirrors the NXDN engine: one `ysf_engine_t` on an
`air_session_t` over a `hal_air_t` provider, with hooks for the call
(`rx_start` / `rx_update` / `rx_end`), voice (`rx_voice` / `tx_voice`, one
9-byte AMBE frame per call) and data (`rx_data` / `tx_data`).

Receive: the generic `air_softmodem` (the FM RX copy at 8 kHz, resampled to
5 samples per symbol, sync fit) plus `air_framer` (YSF sync, 20 symbols,
tolerating up to 2 symbol errors) plus `ysf_rx`, which decodes the FICH and
DCH and hands the voice blocks to the engine. Only communication frames
carry voice; a header or terminator holds DCHs where the voice would be.

Transmit: the sequence builder (`ysf_tx_frame_build`) makes one frame at a
time — header, then communication frames FN 0..7 repeating while PTT is
held, then terminator — and the engine queues its dibits through
`air_session_tx` to the provider's **native 4FSK slot engine**, not through
the TX RAM route. The old design doc planned the TX RAM route (the NXDN
precedent), but the current build's NXDN TX already uses the native slot
engine, so YSF follows it; the host `ysf_tx` streamer's sample output is
not ported.

The engine stages voice in a ring of 10 blocks (two frames): `tx_fill`
collects every block the encoder has made, and a frame is built from the
ring, so the encoder's real-time rate and the frame's five-at-once demand
meet. Blocks already staged for the frame in hand stay staged: a wait is
resumed, never re-fetched, so the encoder's blocks are taken once, in
order. A block the encoder has not made yet is waited for while the
provider's queue holds more than its lead (60 ms plus one chunk); once the
queue runs short, silence goes instead. After the key, a communication
frame is still built while voice the encoder already made waits in the
ring, so the tail of the over is sent, not dropped, and the terminator
follows once the ring is empty. The tail is at most the ring (10 blocks in
sq, 5 staged: 3 frames), so past 10 communication frames after the key the
terminator is forced and the rest of the ring is dropped: a voice source
that never stops cannot hold the transmitter on. The voice stream is
restarted at every transmission (the DMR call does the same), so each over
starts with a live audio callback and a fresh microphone route: the stream
otherwise runs since the channel was selected, and a beep or a stream
takeover can leave it stale or dead and every over goes out silent.

## 6. DG-ID squelch

First cut, to confirm against a real Yaesu radio: a call is taken when the
receiver's DG-ID is 0 (any), or the frame's SC equals it, or SC is 0, or SC
is 127 (Break Out). A DG-ID the squelch does not take drops the frame before
any call starts; a DG-ID change mid-call ends the old call.

## 7. Not written

- The `hrc6000AirTest` provider fit case: the engine test's fake provider
  covers the YSF fit; the CI firmware build is the real check.
- On-air validation of any of it (M2, M4, M5b, M5c in
  `docs/ysf-c4fm.md`).

## 8. The channel kind

The radio glue is written: `RADIO_MODE_YSF`, `CODEPLUG_KIND_YSF` (kind 6),
`core/radio/radio_ysf.c`, the channel editor rows, the mode row, dual watch,
the debug link and zone import. The channel format is
[ysf-codeplug.md](ysf-codeplug.md): a 13-byte tag block on the channel
(data type, deviation, DG-IDs, destination call sign) and a 30-byte
radio-wide identity record (source, downlink and uplink call signs). The
web editor's `mode_ysf.js` is still to be written against that format.
