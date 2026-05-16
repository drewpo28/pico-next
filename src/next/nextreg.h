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
