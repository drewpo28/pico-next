// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: 8K MMU implementation. See mmu.h.

#include "mmu.h"

#include <cstring>

#include "psram_ram.h"
#include "nextreg.h"
#include "../Debug.h"

namespace NextMMU {

uint8_t* slot_ptr[SLOTS] = { nullptr };
bool     slot_ro [SLOTS] = { false };
uint8_t  rom_image[ROM_SIZE];

// Each consecutive 8 KB ROM half maps to one Z80 8K slot when that slot's
// NextReg holds 0xFF. The two ROM halves that a slot selects (high or
// low) are driven by the Memory Mapping Register / Alt-ROM logic; for
// Milestone 1 we treat the entire 64 KB ROM image as flat and map slot
// 0 → rom_image[0..0x1FFF], slot 1 → rom_image[0x2000..0x3FFF], etc. The
// 128K/+3 Alt-ROM swap arrives with the ROM loader commit.
static inline uint8_t* rom_for_slot(int slot) {
    return rom_image + (slot * SLOT_SIZE);
}

void set_slot(int slot, uint8_t page) {
    if (slot < 0 || slot >= SLOTS) return;
    if (page == PAGE_ROM) {
        slot_ptr[slot] = rom_for_slot(slot);
        slot_ro [slot] = true;
        return;
    }
    if (page < NextRAM::PAGE_COUNT && NextRAM::available) {
        slot_ptr[slot] = NextRAM::page_ptr(page);
        slot_ro [slot] = false;
        return;
    }
    // Page outside the documented 0..223 RAM range and not 0xFF. Real Next
    // hardware aliases these to ROM or to optional extra-RAM expansions.
    // Pin the slot to the ROM image so the bus reads as 0xFF instead of
    // dereferencing a null pointer — the CPU will hit RST $38 and that
    // shows up in our trace, which is the behaviour we want during early
    // bring-up.
    slot_ptr[slot] = rom_for_slot(slot);
    slot_ro [slot] = true;
}

void init() {
    // Fill the ROM placeholder so unmapped fetches decode to RST $38.
    memset(rom_image, 0xFF, sizeof(rom_image));
}

void reset() {
    // Default Spectrum 128K-on-Next memory map at power-on (page numbers
    // from the Memory Mapping Register doc — each 16 KB classic bank
    // covers two 8 KB Next pages).
    //
    //   Slot 0 ($0000-$1FFF) = ROM (PAGE_ROM)
    //   Slot 1 ($2000-$3FFF) = ROM (PAGE_ROM)
    //   Slot 2 ($4000-$5FFF) = bank 5 lower → page 10
    //   Slot 3 ($6000-$7FFF) = bank 5 upper → page 11
    //   Slot 4 ($8000-$9FFF) = bank 2 lower → page 4
    //   Slot 5 ($A000-$BFFF) = bank 2 upper → page 5
    //   Slot 6 ($C000-$DFFF) = bank 0 lower → page 0
    //   Slot 7 ($E000-$FFFF) = bank 0 upper → page 1
    static const uint8_t defaults[SLOTS] = {
        PAGE_ROM, PAGE_ROM, 10, 11, 4, 5, 0, 1
    };

    // Mirror the defaults into NextReg $50-$57 so software-visible state
    // matches what set_slot() installs.
    for (int i = 0; i < SLOTS; ++i) {
        NextReg::regs[0x50 + i] = defaults[i];
        set_slot(i, defaults[i]);
    }

    Debug::log("NextMMU: reset — ROM/ROM/RAM5/RAM5/RAM2/RAM2/RAM0/RAM0");
}

} // namespace NextMMU
