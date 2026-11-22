# IRA FW — Independent Radio Architecture Firmware

**Baofeng DM-32UV** firmware built on [RadioFoundry](https://github.com/OpenDigitalRadio/rfoundry) (formerly
OpenDigitalRadio), with a **wideband transmit/extend** edit and an **Irish tactical** rebrand.

> The IRA firmware lives at <https://github.com/IrishPiratePenguin/Independent-Radio-Architecture>. Author:
> **IrishPiratePenguin** (<https://github.com/IrishPiratePenguin>).

> This package is a ready-to-build source tree for the Baofeng **DM-32UV** (HR_C7000 / C-SKY CK803S). It includes
> the toolchain, the one FreeRTOS submodule the build needs, and a prebuilt firmware image. See **Build** below.

## ⚠️ Warning / Disclaimer

> **WARNING: Use this firmware at your own risk (entirely).** There is absolutely no guarantee that it will work in any way shape or form on your radio(s), it may even brick your radio(s), in which case, you'd need to buy another radio. Never transmit on an unauthorized frequency or frequencies, and follow all local rules and regulations.

---

## What this is

- **Base:** RadioFoundry `e89121e9` (commit at the top of this tree) — a single source tree that builds for every
  supported DMR radio. The DM-32UV is one board in it.
- **Wideband TX/RX edit:** the DM-32UV's band table is widened from the stock 136–174 / 400–480 MHz transmit ranges to
  **6 MHz → 1300 MHz transmit** across five contiguous paths, with the **65–136 MHz band left as receive-only**.
- **Irish tactical theme:** black background, green status strip, Irish flag boot splash, `IRA` / `FW` wordmark.
- **Digital baked in:** DMR (incl. P25/NXDN/AES where the platform supports them), FM, AM air-band RX, broadcast FM,
  APRS, GPS.

The band edit is recorded as a reproducible patch in
[`docs/dev/iraw-band-wide-fix.patch`](docs/dev/iraw-band-wide-fix.patch) and described in full in
**Band table / TX–RX split** below.

---

## Quick start

```sh
cd IRA-FW-package
PATH="$PWD/toolchain/bin:$PATH" \
make BOARD=dm32 BUILD=build/dm32 GITVERSION=iraw RFOUNDRY_GIT_TIME=1 all
```

Output: `build/dm32/rfoundry_dm32.bin` (**680 KB**, FLASH 77 %, RAM 97 %).

A prebuilt image from this exact edit is already included in
[`bin/rfoundry_dm32.bin`](bin/rfoundry_dm32.bin) if you just want to flash.

### What the switches mean

| Token | Value | Meaning |
|---|---|---|
| `CROSS` | `toolchain/bin/csky-elf-` | The C-SKY compiler prefix (set on PATH, also passed through `CROSS=`). |
| `BOARD=dm32` | `dm32` | The Baofeng DM-32UV board directory. |
| `BUILD=` | `build/dm32` | Where this board's object files and image are written. |
| `GITVERSION=iraw` | `iraw` | The version stamp printed on the About screen and in the firmware header. |
| `RFOUNDRY_GIT_TIME=1` | `1` | Lets the build run even though the tree is not a fresh git checkout. |

---

## Band table / TX–RX split

The band table lives in [`boards/dm32/board.c`](boards/dm32/board.c). Every entry is a `{ lo_hz, hi_hz, tx }` row
(`band_t` in [`hal/board.h`](hal/board.h)). The transmitter keys (PTT) only on rows where `tx == true`, read through
`board_in_band(hz, true)` in the radio state machine (`core/radio/radio_sm.c`). Rows that are `tx == false` still
receive — they are the **RX gaps** between the transmit paths.

```c
static const band_t bands[] = {
	{ 136000000, 174000000, true },   // VHF TX
	{ 400000000, 480000000, true },   // UHF TX

#if CONFIG_AM_RX
	{ 108000000, 136000000, false },  // air band (AM RX only)
#endif
#if CONFIG_BCAST_FM
	{  65000000, 108000000, false },  // broadcast FM (RDA5807M RX only)
#endif

#if CONFIG_RX_EXT
	// Wideband TX: FD6818 + PA/LNA chain transmit 6 MHz → 1300 MHz across five contiguous paths
	{  6000000, 20000000, true },     // 6–20 MHz   low-band TX   (6 m / 4 m)
	{ 20000000, 42000000, false },    // 20–42 MHz  RX gap
	{ 42000000, 60000000, true },     // 42–60 MHz  2 m TX
	{ 60000000,136000000, false },    // 60–136 MHz RX gap   ★ the 65–136 MHz band you leave out ★
	{136000000,279000000, true },     // 136–279 MHz VHF TX
	{279000000,280000000, false },    // 279–280 MHz RX gap
	{280000000,520000000, true },     // 280–520 MHz UHF TX
	{520000000,690000000, false },    // 520–690 MHz RX gap
	{690000000,1300000000,true },     // 690–1300 MHz high-band TX (23 cm + UHF-G)
	{1300000000,1740000000,false};    // 1300–1740 MHz RX tail
#endif
};
```

### The split, as a band map

| Band (MHz) | Direction | Row `tx` | Notes |
|---:|:--:|:--:|---|
| 6 – 20   | TX  | `true` | Low-band path (6 m / 4 m) |
| 20 – 42  | RX  | `false` | Gap between low-band and 2 m |
| 42 – 60  | TX  | `true` | 2 m path |
| **60 – 136** | **RX** | **`false`** | **★ The 65–136 MHz band is left out of TX (receive-only) ★** — spans 6 m, 4 m, CB, the air band and broadcast FM |
| 136 – 279 | TX | `true` | VHF path |
| 279 – 280 | RX | `false` | Gap between VHF and UHF paths |
| 280 – 520 | TX | `true` | UHF path |
| 520 – 690 | RX | `false` | Gap above the UHF path |
| 690 – 1300 | TX | `true` | High-band path (23 cm + UHF-G) |
| 1300 – 1740 | RX | `false` | RX tail past the high-band path |

**The 65–136 MHz gap is intentional.** Per the request, transmit stops being keyed there — 60–136 MHz is receive-only.
Everything below 60 MHz (broadcast FM, air-band AM) was always RX-only in the stock table and stays that way. The TX
paths are the five contiguous `true` rows: 6–20, 42–60, 136–279, 280–520, 690–1300 MHz.

### `rx_ext_count`

The rows under `#if CONFIG_RX_EXT` (the wideband set) are the *receive-only extension* — enabled in Settings >
Advanced, off by default. `board_band_count()` hides `rx_ext_count` rows while it is off, so it **must match the number
of wideband rows**. It is now **10** (see [`boards/dm32/board.c`](boards/dm32/board.c), `.rx_ext_count = 10`).

### Why 1300 MHz, not 1740?

The FD6818 RF transceiver + the PA/LNA chain pass 6 MHz → 1300 MHz ([`docs/spec/hardware-dm32.md:725`](docs/spec/hardware-dm32.md#83-frequency-band-state-bandwidth-bandwidth)).
The rows above 1300 MHz (1300–1740) are RX tail only — the transmitter and power amplifier are not rated that high.

---

## Irish tactical theme

- **Source of truth for the rebrand:** [`boards/dm32/board.c`](boards/dm32/board.c) (band split) and
  [`core/ui/ui_screens.c`](core/ui/ui_screens.c) (theme 0 colours + splash + `IRA` / `FW` wordmark).
- The theme is gated behind `CONFIG_UI_THEME_IRISH` in [`core/config.h`](core/config.h). When on, the boot splash
  draws the Irish flag, the About screen reads **IRA FW / IrishPiratePenguin**, and the default UI is black with a green
  status strip.
- Colours (theme 0, RGB565): `bg(0,0,0)`, `fg(255,255,255)`, `rx(40,184,72)`, `tx(224,32,32)`, `warn(240,168,0)`,
  `focus(32,96,200)`. The green top strip = `accent_apply()` → `strip_bg(0,160,48)` when `ui_theme == 0`.
- **Theme menu** (Settings > Display > Theme), with the kawaii theme on:
  - **Irish** (theme 0) — black UI, Irish flag splash, green status strip. Labeled "Irish" (previously "Pink").
  - **Pink** (theme 1) — the stock pink kawaii palette. Was the old "Light" row; relabeled here.
  - **High contrast** (theme 2) — pure black and white, saturated meaning colors.

---

## Build

### Requirements

| | |
|---|---|
| **MCU platform** | `hrc7000` — C-SKY **CK803S** |
| **Compiler** | `csky-elf-gcc` (v14.3.0), **included** in `toolchain/bin/` |
| **FreeRTOS** | `third_party/FreeRTOS-Kernel/` (pre-checked-out; one submodule) |
| **Make, Python 3** | standard |

The `toolchain/` directory (≈322 MB) is a **prebuilt, self-contained** C-SKY toolchain — it is shipped so the tree
builds offline without running `tools/build-csky-toolchain.sh` (a ~45 minute from-source build). If you prefer a small
tree, delete `toolchain/` and install a system `csky-elf-gcc` instead (see below).

### From the package (offline, includes toolchain)

```sh
cd IRA-FW-package
export PATH="$PWD/toolchain/bin:$PATH"
CROSS=toolchain/bin/csky-elf-
make BOARD=dm32 CROSS=$CROSS BUILD=build/dm32 GITVERSION=iraw RFOUNDRY_GIT_TIME=1 all
# -> build/dm32/rfoundry_dm32.bin
```

### With a system toolchain (no `toolchain/`)

```sh
# install csky-elf-gcc to your PATH (see tools/build-csky-toolchain.sh or tools/docker/csky-toolchain/)
export PATH="/opt/csky-elf/bin:$PATH"
make BOARD=dm32 CROSS=csky-elf- BUILD=build/dm32 GITVERSION=iraw RFOUNDRY_GIT_TIME=1 all
```

### Common gotchas

- **`as: unrecognized option '-mcpu=ck803s'`** → the host assembler was used because `toolchain/bin` was not on `PATH`.
  Put it on `PATH` (see above).
- **`git clean -fdx` wipes `third_party/FreeRTOS-Kernel/include/FreeRTOS.h`** → re-check it out before rebuilding.
- **Band-tx unlock vs. the first-boot gate:** the band table allows wideband TX, but `radio_dm32.c` has a first-boot
  `FB_BOARD_TX` gate (`rf_unlocked`) that pins initial TX to one frequency until the radio is unlocked. To key PTT
  wideband out of the box, open that gate too. (Documented in the notes below.)

### Flashing

`hrc7000` boards flash through the vendor bootloader over the programming cable:

```sh
tools/hrc7000_codec.py build/dm32/rfoundry_dm32.bin      # merge codec, if you have a stock file
tools/serial_loader.py build/dm32/rfoundry_dm32.bin       # write it through the vendor bootloader
```

Without a codec the firmware runs but FM-only. See [`docs/dev/building.md`](docs/dev/building.md#flashing).

---

## Package layout

```
IRA-FW-package/
├── README.md                 ← you are here
├── bin/rfoundry_dm32.bin      ← prebuilt firmware (this edit)
├── boards/dm32/               ← band table, radio init, theme
│   └── board.c                ← ★ the wideband TX/RX edit ★
├── core/                      ← radio state machine, UI (theme, splash, wordmark)
├── hal/                       ← board_t, band_t, board_in_band(), board_band_count()
├── drivers/                   ← FD6818 RF, HRC7000 modem, ST7789 display, RDA5807M bcast FM
├── third_party/FreeRTOS-Kernel/  ← the one FreeRTOS submodule the build needs
├── toolchain/                 ← prebuilt csky-elf-gcc (≈322 MB; optional)
├── docs/
│   ├── spec/hardware-dm32.md  ← the radio's hardware reference (bands, FD6818 range)
│   └── dev/
│       └── iraw-band-wide-fix.patch  ← the band edit, as a reproducible diff
└── tools/                     ← build, codec, CI scripts
```

---

## Files

| File | Role |
|---|---|
| `boards/dm32/board.c` | The band table (TX/RX split) + `board_t` descriptor. **The wideband edit lives here.** |
| `core/ui/ui_screens.c` | Theme 0 colours, Irish flag splash, `IRA` / `FW` wordmark, About screen. |
| `core/config.h` | `CONFIG_UI_THEME_IRISH` gate (default 1 = Irish on). |
| `hal/board.h` | `band_t`, `board_in_band(hz, need_tx)`, `board_band_count()`, `rx_ext_count` semantics. |
| `core/radio/radio_sm.c` | `tx_check()` → `board_in_band(hz, true)` gates PTT on TX rows. |
| `boards/dm32/radio_dm32.c` | Radio init, power calibration, first-boot `rf_unlocked` TX gate. |
| `docs/spec/hardware-dm32.md` | The DM-32UV hardware reference (FD6818 6–1300 MHz band, §8.3). |
| `docs/dev/iraw-band-wide-fix.patch` | The band edit as a reproducible diff from base `e89121e9`. |
| `docs/dev/building.md` | RadioFoundry build/flash/toolchain reference. |

---

## Attribution

Base firmware is MIT-licensed RadioFoundry — see [`LICENSE`](LICENSE) and [`AUTHORS`](AUTHORS). This package adds:

- Copyright (c) 2026 **The RadioFoundry Authors** — wideband band-table edit and IRISH firmware rebrand.
- Copyright (c) 2026 **IrishPiratePenguin** — IRA FW (Independent Radio Architecture Firmware) rebrand, wideband
  band split and this packaging. <https://github.com/IrishPiratePenguin>

> **Clean-room note.** RadioFoundry is a clean-room build (no copy of any aftermarket firmware). The AMBE codec and
> stock firmware images are *not* included here — place them in `codecs/` if you want the codec-merged image.
> See [`docs/CLEANROOM.md`](docs/CLEANROOM.md).

---

## Build log

Full compiler + linker output for the build that produced the prebuilt `bin/rfoundry_dm32.bin` is in
[`docs/dev/iraw-band-wide-fix.patch`](docs/dev/iraw-band-wide-fix.patch) (the band edit) plus the Makefile's
`build/dm32/rfoundry_dm32.map` section sizes:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      695324 B       880 KB     77.16%
             RAM:      205480 B     211456 B     97.17%
```

---

## See also

- [`docs/spec/hardware-dm32.md`](docs/spec/hardware-dm32.md#83-frequency-band-state-bandwidth-bandwidth) — FD6818 band range.
- [`docs/dev/building.md`](docs/dev/building.md) — toolchains, switches, flashing.
- [`docs/dev/contributing.md`](docs/dev/contributing.md) — the RadioFoundry contribution rules.
- [`AGENTS.md`](AGENTS.md) — notes for coding agents working in this tree.
