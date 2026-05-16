# pico-next Project Memory

This repository is `pico-next` — an in-progress port of the [pico-spec](https://github.com/DnCraptor/pico-spec) ESPectrum emulator to the **ZX Spectrum Next** architecture, targeting RP2350 boards with PSRAM. Spec: [SpecNext Wiki](https://wiki.specnext.dev/Main_Page). Roadmap: [docs/PLAN.md](docs/PLAN.md).

Strategy (confirmed with user): preserve the pico-spec source structure, add Next-specific modules under `src/next/`. Existing modules (`Z80_JLS`, `MemESP`, `Video`, `AySound`, `Ports`, etc.) are extended with Next features behind a `PICO_NEXT_FEATURES` build flag. Classic Spectrum 48K/128K/Pentagon compatibility mode is preserved.

Development branch: **`claude/pico-next-cleanup-JtWGm`** — all Next-related work is committed here, not on `main`.

Notes below are inherited from pico-spec and describe the classic-Spectrum subsystems that pico-next is built upon.

---

## GPIO Map (all boards)

### Classification

- **FIXED** — hardwired on PCB, cannot change (display, SD card, onboard PSRAM, LED)
- **REASSIGNABLE** — currently used but can be remapped/disabled in software (NESPAD, keyboard, audio, WAV-input)
- **FREE** — not used by any peripheral

### MURM2 (RP2350A, GPIO 0-29) — FIXED=15, REASSIGNABLE=9, FREE=5

| GPIO | Function | Cat | Notes |
|------|----------|-----|-------|
| 0 | — | FREE | |
| 1 | — | FREE | |
| 2 | KBD_CLOCK | REASSIGN | PS/2 keyboard |
| 3 | KBD_DATA | REASSIGN | PS/2 keyboard |
| 4 | SD MISO | FIXED | SPI0 PCB |
| 5 | SD CS | FIXED | SPI0 PCB |
| 6 | SD SCK | FIXED | SPI0 PCB |
| 7 | SD MOSI | FIXED | SPI0 PCB |
| 8 | BUTTER_PSRAM | FIXED | RP2350 onboard PSRAM XIP CS1 |
| 9 | Audio DATA/BEEPER/LATCH_595 | REASSIGN | Triple-alias (I2S/PWM/595) |
| 10 | Audio BCK/PWM0/CLK_595 | REASSIGN | Triple-alias |
| 11 | Audio LCK/PWM1/DATA_595 | REASSIGN | Triple-alias |
| 12-19 | VGA/HDMI (8 pins) | FIXED | Display base=12; also PSRAM SPI on 18-19 |
| 20 | PSRAM_MOSI / NES_CLK | FIXED | PSRAM priority; **NESPAD conflict!** |
| 21 | PSRAM_MISO / NES_LAT | FIXED | PSRAM priority; **NESPAD conflict!** |
| 22 | LOAD_WAV_PIO | REASSIGN | Real tape input |
| 23 | — | FREE | |
| 24 | — | FREE | |
| 25 | LED | FIXED | |
| 26 | NES_DATA | REASSIGN | NESPAD joy1 |
| 27 | NES_DATA+1 (implicit) | REASSIGN | NESPAD joy2 (PIO reads DATA+1) |
| 28 | — | FREE | |
| 29 | CLK_AY_PIN2 | REASSIGN | AY clock out |

### PICO_PC (RP2350A, GPIO 0-29) — FIXED=13, REASSIGNABLE=10, FREE=6

| GPIO | Function | Cat | Notes |
|------|----------|-----|-------|
| 0 | KBD_CLOCK | REASSIGN | |
| 1 | KBD_DATA | REASSIGN | |
| 2 | — | FREE | QWST1 connector |
| 3 | — | FREE | |
| 4 | SD MISO | FIXED | SPI0 PCB |
| 5 | NES_CLK / LOAD_WAV_PIO | REASSIGN | Shared |
| 6 | SD SCK | FIXED | SPI0 PCB |
| 7 | SD MOSI | FIXED | SPI0 PCB |
| 8 | BUTTER_PSRAM | FIXED | RP2350 onboard PSRAM XIP CS1 |
| 9 | NES_LAT | REASSIGN | |
| 10 | — | FREE | |
| 11 | — | FREE | |
| 12-19 | VGA/HDMI (8 pins) | FIXED | Display base=12 |
| 20 | NES_DATA | REASSIGN | UXT1-3 |
| 21 | NES_DATA2 | REASSIGN | UXT1-4 |
| 22 | SD CS | FIXED | SPI0 PCB |
| 23 | — | FREE | |
| 24 | — | FREE | |
| 25 | LED | FIXED | |
| 26 | BEEPER/LATCH_595 | REASSIGN | |
| 27 | PWM0/CLK_595 | REASSIGN | Audio PWM right |
| 28 | PWM1/DATA_595 | REASSIGN | Audio PWM left |
| 29 | CLK_AY_PIN2 | REASSIGN | AY clock out |

### PICO_DV (RP2350, GPIO 0-29 + 47) — FIXED=14, REASSIGNABLE=8, FREE=9

| GPIO | Function | Cat | Notes |
|------|----------|-----|-------|
| 0 | KBD_CLOCK | REASSIGN | |
| 1 | KBD_DATA | REASSIGN | |
| 2 | — | FREE | |
| 3 | — | FREE | |
| 4 | — | FREE | |
| 5 | SD SCK | FIXED | SPI1 PCB |
| 6-13 | VGA/HDMI (8 pins) | FIXED | Display base=6; NES_CLK=8, NES_LAT=9 conflict! |
| 14 | — | FREE | |
| 15 | — | FREE | |
| 16 | — | FREE | |
| 17 | — | FREE | |
| 18 | SD MOSI | FIXED | SPI1 PCB |
| 19 | SD MISO | FIXED | SPI1 PCB |
| 20 | LOAD_WAV_PIO / NES_DATA | REASSIGN | No USE_NESPAD |
| 21 | NES_DATA2 | REASSIGN | |
| 22 | SD CS | FIXED | SPI1 PCB |
| 23 | — | FREE | |
| 24 | — | FREE | |
| 25 | LED | FIXED | |
| 26 | Audio DATA/PWM0/LATCH_595 | REASSIGN | |
| 27 | Audio BCK/PWM1/CLK_595 | REASSIGN | |
| 28 | Audio LCK/BEEPER/DATA_595 | REASSIGN | |
| 29 | CLK_AY_PIN2 | REASSIGN | |
| 47 | BUTTER_PSRAM | FIXED | RP2350B onboard PSRAM |

### ZERO2 (RP2350B, GPIO 0-47) — FIXED=14, REASSIGNABLE=12, FREE=22

| GPIO | Function | Cat | Notes |
|------|----------|-----|-------|
| 0-1 | — | FREE | PSRAM disabled |
| 2 | PCM5122_I2C_SDA | REASSIGN | DAC control (if attached) |
| 3 | PCM5122_I2C_SCL | REASSIGN | |
| 4-6 | — | FREE | NESPAD disabled |
| 7 | CLK_AY_PIN2 | REASSIGN | AY clock out |
| 8-9 | — | FREE | |
| 10 | Audio DATA/PWM0/LATCH_595 | REASSIGN | |
| 11 | Audio BCK/PWM1/CLK_595 | REASSIGN | |
| 12 | Audio LCK/BEEPER/DATA_595 | REASSIGN | |
| 13 | — | FREE | |
| 14 | KBD_CLOCK | REASSIGN | Moved from 2/3 for PCM5122 I2C |
| 15 | KBD_DATA | REASSIGN | |
| 16 | — | FREE | |
| 17 | LOAD_WAV_PIO | REASSIGN | WAV loader |
| 18 | PCM5122_I2S_BCK | REASSIGN | DAC bit clock |
| 19 | PCM5122_I2S_LCK | REASSIGN | DAC LR clock |
| 20 | — | FREE | |
| 21 | PCM5122_I2S_DATA | REASSIGN | DAC data |
| 22 | — | FREE | |
| 23-29 | — | FREE | |
| 30 | SD SCK | FIXED | SPI1 Waveshare board |
| 31 | SD MOSI | FIXED | |
| 32-39 | VGA/HDMI (8 pins) | FIXED | Display base=32 |
| 40 | SD MISO | FIXED | |
| 41-42 | — | FREE | |
| 43 | SD CS | FIXED | |
| 44-46 | — | FREE | |
| 47 | BUTTER_PSRAM | FIXED | RP2350B onboard PSRAM |

### Summary

| Board | MCU | GPIO | FIXED | REASSIGN | FREE | NESPAD |
|-------|-----|------|-------|----------|------|--------|
| MURM2 | RP2350A | 0-29 | 15 | 9 | 5 | **Conflict with PSRAM** (CLK=20, LAT=21) |
| PICO_PC | RP2350A | 0-29 | 13 | 10 | 6 | OK |
| PICO_DV | RP2350 | 0-29,47 | 14 | 8 | 9 | Conflict with display (8,9) |
| ZERO2 | RP2350B | 0-47 | 14 | 12 | 22 | Disabled |

### Known bugs and conflicts

1. **MURM2 NESPAD vs PSRAM** — NES_CLK=20, NES_LAT=21 overlap PSRAM_MOSI=20, PSRAM_MISO=21. Cannot coexist
2. **PICO_DV NESPAD vs Display** — NES_CLK=8, NES_LAT=9 inside display range (6-13). USE_NESPAD correctly not set

## Tools

- `tools/z80disasm.py` — Z80 disassembler (pure Python3, no deps)
  - TAP files: `python3 tools/z80disasm.py input.tap` (auto-parses headers/blocks)
  - Raw binaries: `python3 tools/z80disasm.py code.bin --org 0x8000`
  - API: `from tools.z80disasm import disasm_bytes, disasm_bytes_text`
  - Supports all Z80 prefixes: CB, DD, FD, ED, DD CB, FD CB (including undocumented)

## Test Files

- `FPGA48all.tap` — SAA1099 test program for ZX Spectrum 128K
  - Disassembly: `FPGA48all_disasm.txt`
  - Loader at 0x5E00, screen at 0x4000, main code at 0x6200
  - Main code starts with `CALL 0x817E` (IM 2 setup), uses SAA1099 port 0x01FE
