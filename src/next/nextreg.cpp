// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: ZX Spectrum Next NextReg-register controller.
// See nextreg.h for module overview.

#include "nextreg.h"
#include "mmu.h"

namespace NextReg {

bool enabled = false;
uint8_t regs[256];
uint8_t selected = 0;

// Board / core identification values reported through the NextReg space.
// Real Next reports a Machine ID of 0x08 (ZX Spectrum Next) and a core
// version that bumps with every FPGA release; we report 3.2 here because
// it's the most recent stable core at the time pico-next is being written
// and software gating on core version mostly checks >=3.x.
//
// Refs:
//   https://wiki.specnext.dev/Board_feature_control
//   $00 Machine ID    -> 0x08 = ZX Spectrum Next
//   $01 Version       -> high nibble = major, low nibble = minor (3.2 = 0x32)
//   $0E Sub-minor     -> patch level (1)
static constexpr uint8_t MACHINE_ID    = 0x08;
static constexpr uint8_t CORE_VERSION  = 0x32;
static constexpr uint8_t CORE_SUBMINOR = 0x01;

void reset() {
    for (int i = 0; i < 256; ++i) regs[i] = 0;
    regs[0x00] = MACHINE_ID;
    regs[0x01] = CORE_VERSION;
    regs[0x0E] = CORE_SUBMINOR;
    selected = 0;
}

uint8_t read(uint8_t reg) {
    return regs[reg];
}

void write(uint8_t reg, uint8_t value) {
    regs[reg] = value;

    // MMU bank slots $50-$57 — update NextMMU pointer cache so subsequent
    // Z80 fetches/reads/writes see the new mapping immediately.
    if (reg >= 0x50 && reg <= 0x57) {
        NextMMU::set_slot(reg - 0x50, value);
        return;
    }

    // Palette $40-$44, CPU speed $07, reset $02, ULA control $68, Alt ROM
    // $8C, ... wire in as their respective modules land.
}

void writeSelect(uint8_t reg) {
    selected = reg;
}

void writeData(uint8_t value) {
    write(selected, value);
}

} // namespace NextReg
