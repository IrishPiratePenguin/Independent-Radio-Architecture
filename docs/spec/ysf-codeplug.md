<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Copyright (c) 2026 lyra XE1POC (changes) -->

# YSF in the codeplug

The stored records of YSF conventional channels, on the generic layer of [channel-modes.md](channel-modes.md).
Little endian throughout. The firmware's codec is `core/ysf/ysf_codeplug.c` (`CONFIG_YSF`). The radio core runs a
channel of kind 6 where the build has `CONFIG_YSF` and the board's air interface carries YSF ([ysf.md](ysf.md) 8);
elsewhere it is parked ([channel-modes.md](channel-modes.md) 6).

| Id | Kind | System type |
|---|---|---|
| 6 | YSF conventional | none |

## 1. Radio-wide record (kind 6)

Key 0x0A00 + 6 ([channel-modes.md](channel-modes.md) 5.5): the operator's call signs, one radio-wide set. A call sign
is 10 ASCII characters, space padded; a byte 0x00 reads as a space, any other byte outside 0x20-0x7E reads as a
space, and trailing spaces are trimmed when read, so a short or absent field reads as empty (all spaces on air).

| Offset | Size | Field |
|---|---|---|
| 0 | 10 | Source call sign (Src): this radio's own call sign, as Yaesu's MYCALL |
| 10 | 10 | Downlink call sign (Down) |
| 20 | 10 | Uplink call sign (Up) |

The remote call signs (Rem1-4) are not stored: a handheld does not originate them, so they go out as spaces.
Nothing else is stored radio-wide: the DG-IDs, the data type and the deviation are the channel's (2).
The VFO also uses these call signs. A nonempty temporary VFO call sign overrides its corresponding field.

## 2. Conventional channel (kind 6)

The tag ([channel-modes.md](channel-modes.md) 2): no system, flag bit 0 transmit allowed. The parameter block:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | Bits 0-1 DT: 0 V/D mode 1, 1 Data FR, 2 V/D mode 2 (DN), 3 Voice FR (VW). Bit 2 Dev: 0 Wide, 1 Narrow. Bits 3-7: 0 |
| 1 | 1 | RX DG-ID, 0 to 127; 0 = any DG-ID (2.1). Above 127 reads as 0 |
| 2 | 1 | TX DG-ID: bit 7 set = bits 6-0 hold it (0 to 127); clear = the RX DG-ID |
| 3 | 10 | Destination call sign (Dest), 10 ASCII, space padded; all spaces (or all zero) reads as `ALL` |

A zeroed block, or none, is V/D mode 1, Wide deviation, any DG-ID received and DG-ID 0 (none) sent, destination
`ALL`; with the tag's flag bit 0 clear, receive only. A block shorter than 13 octets reads with its missing fields
at their defaults (a short destination reads with the missing characters as spaces). Both deviations use 12.5 kHz
channel settings ([ysf.md](ysf.md) 1).

The base record carries:

| Record field | Use |
|---|---|
| Name, RX and TX frequency, power, TX time-out, scan skip | As any channel |
| Contact | Kept, not read: a YSF call has no numeric talk target; the DG-ID (2.1) and the destination call sign select it |
| Per-channel DMR ID | Kept, not read: a YSF station is its call sign, not a number |
| RX group list | Kept, not read |
| Mode, receive only | DMR, receive only, always (the safe stand-in) |

There is no talkaround setting: YSF frames are inbound either way. The record's direct-mode bit is kept and not
read. The call mode (CM) sent is always group and the relay mode (MR) always direct: the DG-ID already selects the
group, and MR is the repeater's to set on the downlink, not the handheld's on the uplink.

### 2.1 DG-ID squelch

The DG-ID is the FICH's SC field (7 bits): 0 none, 1-126 codes, 127 Break Out ([ysf.md](ysf.md) 2, 6). A call is
taken when the receiver's DG-ID is 0 (any), or the frame's SC equals it, or SC is 0, or SC is 127. A call the
squelch does not take drops the frame before any call starts; a DG-ID change mid-call ends the old call. The
receiver's DG-ID of 0 hears every DG-ID; a TX DG-ID of 0 sends SC 0 (none).

### 2.2 Data type and deviation

DT is what the channel transmits, and the voice path it expects: V/D mode 1 and DN carry half-rate voice (49
AMBE+2 parameter bits per 20 ms block), Data FR carries data, Voice FR carries full-rate voice, which this build
receives and sends with the voice blocks zero ([ysf.md](ysf.md) 4). Reception takes any DT: a frame's own DT
decides its voice path. Dev is the deviation of the channel, Wide (2700 Hz) or Narrow (1350 Hz), for transmit and
receive alike. A new DT or a new deviation is a new session: the engine starts over.

### 2.3 Call signs

Why the destination is the channel's and the rest is the radio's: a channel addresses a station or a repeater, so
its destination changes with the channel, while the source, downlink and uplink call signs are this radio's
identity, the same on every channel, as Yaesu's MYCALL is one per radio. The editor writes the destination with
the channel and the three radio-wide call signs with the radio's YSF identity; the radio's channel editor writes
the destination the same way, and its YSF settings (the debug link's) hold the three for the VFO. A destination
of all spaces reads as `ALL`, the group destination, and is written back as `ALL` followed by spaces.

### 2.4 Writing from the radio

A YSF channel the radio saves (the VFO in YSF stored as a channel, a mode changed for now made permanent) is
written as [channel-modes.md](channel-modes.md) 2.4 gives: the base record as the safe stand-in, then a new tag of
kind 6 with no system, transmit allowed from the channel and a full parameter block from the YSF settings: the
data type, the deviation, the RX and TX DG-IDs, and the destination call sign. The settings' source, downlink and
uplink call signs are not stored: they are the radio-wide record's. Editing a channel that is YSF already rewrites
its record and lets the tag follow, the block kept byte for byte; a copy at another number takes the source's tag.
When the channel editor changed the DG-ID, the data type, the deviation or the destination
([ui.md](ui.md) 2.1), the record is written as the safe stand-in and the tag follows it, then the edited block is
written over the front of the tag's ([channel-modes.md](channel-modes.md) 2.4): the system, the fallback, the other
flags and any later bytes of the block stay, and under a tag with the base-record flag the record keeps its own
mode. A copy takes its source's tag first. A plain channel in YSF takes the edited block in place of the
settings'.

## 3. Sharing

A zone share carries a YSF channel's tag as it is ([zone-share.md](zone-share.md) 4.2): the block is 13 octets,
inside the share's limit of 64.
