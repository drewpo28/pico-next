// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: ZX Spectrum Next NextReg-register controller.
// See nextreg.h for module overview.

#include "nextreg.h"
#include "mmu.h"
#include "layer2.h"
#include "../ESPectrum.h"
#include "../CPU.h"

namespace NextReg {

uint16_t palette[PALETTE_COUNT][PALETTE_ENTRIES];
uint8_t  palette_select       = 0;
uint8_t  palette_index        = 0;

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

    // Palette state — zero everything; the ROM rewrites whatever it needs
    // during boot. Storing as 9-bit values (R3 G3 B3) so consumers don't
    // need to redo the 8↔9-bit unpacking on every render.
    for (int p = 0; p < PALETTE_COUNT; ++p)
        for (int i = 0; i < PALETTE_ENTRIES; ++i)
            palette[p][i] = 0;
    palette_select       = 0;
    palette_index        = 0;
}

uint8_t read(uint8_t reg) {
    return regs[reg];
}

// Resolve NextReg $43 bits 6-4 to the [0..7] index used by palette[].
static inline uint8_t decode_palette_select(uint8_t r43) {
    // bits 6-4 hold the encoding: 000=ULA1, 100=ULA2, 001=L2-1, 101=L2-2,
    // 010=Spr-1, 110=Spr-2, 011=Tm-1, 111=Tm-2. Compress into 0..7 by
    // grouping bits as (bit6 << 2) | (bits 5-4).
    return (uint8_t)(((r43 >> 6) & 0x01) << 2 | ((r43 >> 4) & 0x03));
}

void write(uint8_t reg, uint8_t value) {
    regs[reg] = value;

    // MMU bank slots $50-$57 — update NextMMU pointer cache so subsequent
    // Z80 fetches/reads/writes see the new mapping immediately.
    if (reg >= 0x50 && reg <= 0x57) {
        NextMMU::set_slot(reg - 0x50, value);
        return;
    }

    // NextReg $07 — programmable CPU speed (low 2 bits, 1× / 2× / 4× / 8×
    // base of 3.5 MHz → 3.5 / 7 / 14 / 28 MHz). Maps directly to the
    // existing ESPectrum::multiplicator that the manual Turbo hotkey
    // also drives (statesInFrame <<= multiplicator), so software
    // requesting 28 MHz via NEXTREG \$07,3 gets the same per-frame
    // T-state budget Alt-F2 Turbo gives.
    // Ref: https://wiki.specnext.dev/Turbo_Control_Register
    if (reg == 0x07) {
        const uint8_t mult = value & 0x03;
        if (ESPectrum::multiplicator != mult) {
            ESPectrum::multiplicator = mult;
            CPU::updateStatesInFrame();
        }
        return;
    }

    // -----------------------------------------------------------------
    // Palette I/O (Spectrum Next 256-colour ULA / Layer2 / Sprite / Tilemap)
    // Refs: https://wiki.specnext.dev/Palettes
    //
    // $40: palette index (8 bits). Bare write — no side effects.
    // $41: 8-bit palette value (RRRGGGBB). Stores into selected palette
    //      at `palette_index`, then auto-increments the index.
    // $43: palette select (bits 6-4 choose which of the 8 palettes is
    //      addressed; other bits hold per-layer enable flags that other
    //      modules consume).
    // $44: 9-bit extension — bit 0 of the byte is the LSB of B, bit 7
    //      is the priority. Updates the in-place colour at the current
    //      index then auto-increments. We treat $44 as a one-shot that
    //      flips the pending flag; real hardware behaves the same.
    // -----------------------------------------------------------------
    if (reg == 0x40) {
        palette_index = value;
        return;
    }
    if (reg == 0x41) {
        uint8_t r = (value >> 5) & 0x07;
        uint8_t g = (value >> 2) & 0x07;
        uint8_t b = (value << 1) & 0x06; // B[2..1] from value[1..0]; B[0]=0
        palette[palette_select][palette_index] = (uint16_t)((r << 6) | (g << 3) | b);
        palette_index++;
        return;
    }
    if (reg == 0x43) {
        palette_select = decode_palette_select(value);
        return;
    }
    if (reg == 0x44) {
        uint16_t cur = palette[palette_select][palette_index];
        // Bit 0 of value = B[0]; bit 7 = priority (stored in bit 9 of entry).
        uint16_t b_lsb = (value & 0x01) ? 0x01 : 0x00;
        uint16_t prio  = (value & 0x80) ? 0x200 : 0x000;
        palette[palette_select][palette_index] =
            (uint16_t)((cur & ~0x201) | b_lsb | prio);
        palette_index++;
        return;
    }

    // -----------------------------------------------------------------
    // Layer 2 state ($12-$18, $70-$72). Storage only — Video.cpp's
    // composite path consumes these on the next scanline.
    // -----------------------------------------------------------------
    switch (reg) {
        case 0x12: Layer2::start_page        = value; return;
        case 0x13: Layer2::start_page_shadow = value; return;
        case 0x14: Layer2::palette_offset    = (uint8_t)(value & 0xF0); return;
        case 0x16: Layer2::scroll_x = (Layer2::scroll_x & 0x0100) | value; return;
        case 0x17: Layer2::scroll_y = value; return;
        case 0x18: Layer2::writeClip(value); return;
        case 0x70:
            // bits 5-4 select the mode; bits 3-0 hold the palette-offset
            // for 320/640 modes (we mirror them into palette_offset).
            switch ((value >> 4) & 0x03) {
                case 0: Layer2::mode = Layer2::MODE_256x192; break;
                case 1: Layer2::mode = Layer2::MODE_320x256; break;
                case 2: Layer2::mode = Layer2::MODE_640x256; break;
                default: Layer2::mode = Layer2::MODE_256x192; break;
            }
            return;
        case 0x71:
            // Bit 0 = scroll_x bit 8 (for 320/640 modes that need 9-bit X).
            Layer2::scroll_x = (uint16_t)((Layer2::scroll_x & 0x00FF) |
                                          ((value & 0x01) << 8));
            return;
        case 0x72: Layer2::scroll_y = value; return;
    }

    // CPU speed $07, reset $02, ULA control $68, Alt ROM $8C, ... wire in
    // as their respective modules land.
}

void writeSelect(uint8_t reg) {
    selected = reg;
}

void writeData(uint8_t value) {
    write(selected, value);
}

} // namespace NextReg
