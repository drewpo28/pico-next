// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: ZX Spectrum Next NextReg-register controller.
//
// NextReg is the Spectrum Next's 256-register configuration space, accessed
// via I/O ports $243B (select) and $253B (data), or via the Z80N NEXTREG
// opcode. This module is the storage and read/write dispatch for those
// registers; module-specific side effects (MMU paging, palette routing,
// CPU speed change, reset) are wired in by the consumer modules in later
// commits and remain pure-table for registers without side effects.
//
// Reference: https://wiki.specnext.dev/Board_feature_control

#pragma once

#include <stdint.h>

namespace NextReg {

// Master enable: true when the emulator is running as a ZX Spectrum Next.
// Classic-Spectrum compatibility modes leave this false so I/O on $243B/$253B
// remains free for legacy use. Set by ESPectrum::setup()/reset() based on
// Config::arch.
extern bool enabled;

// 256-entry register file. Public for fast read access from hot paths.
extern uint8_t regs[256];

// Currently selected register (the value written to port $243B).
extern uint8_t selected;

// Re-initialise the register file to power-on defaults.
void reset();

// Spectrum Next has eight 256-entry palettes (ULA / Layer2 / Sprites /
// Tilemap × first/second). Each entry stores a 9-bit colour (RRR GGG BBB)
// plus a priority bit, but software can write either 8-bit (port $41) or
// 9-bit (port $44) forms. The palette currently selected for read/write
// is encoded in NextReg $43 bits 6-4.
//
// Refs: https://wiki.specnext.dev/Palettes
constexpr int PALETTE_COUNT = 8;
constexpr int PALETTE_ENTRIES = 256;

// 9-bit colour value + priority bit packed in low 10 bits of uint16_t.
//   bits 8-6  = R (3 bits)
//   bits 5-3  = G (3 bits)
//   bits 2-0  = B (3 bits)
//   bit  9    = priority (Layer2-over-sprite or similar)
// Public so Video.cpp can index without indirection.
extern uint16_t palette[PALETTE_COUNT][PALETTE_ENTRIES];

// Palette selector decoded from NextReg $43 bits 6-4. 0=ULA1, 4=ULA2,
// 1=L2-1, 5=L2-2, 2=Spr-1, 6=Spr-2, 3=Tm-1, 7=Tm-2.
extern uint8_t  palette_select;

// Auto-incrementing index for $41/$44 writes (NextReg $40).
extern uint8_t  palette_index;

// Port $243B (write): select a register for subsequent $253B access.
void writeSelect(uint8_t reg);

// Port $243B (read): return the currently selected register number.
// Real hardware returns the selected index; we mirror that.
inline uint8_t readSelect() { return selected; }

// Port $253B (write): write to the currently selected register.
void writeData(uint8_t value);

// Port $253B (read): read the currently selected register.
inline uint8_t readData() { return regs[selected]; }

// Direct register access (used by Z80N NEXTREG opcode and by other
// emulator modules that need to read/write a specific register without
// touching the $243B-selected state).
uint8_t read(uint8_t reg);
void write(uint8_t reg, uint8_t value);

} // namespace NextReg
