// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: 8K MMU implementation. See mmu.h.

#include "mmu.h"

#include <cstring>

#include "psram_ram.h"
#include "nextreg.h"
#include "../MemESP.h"
#include "../Debug.h"

namespace NextMMU {

uint8_t* slot_ptr[SLOTS] = { nullptr };
bool     slot_ro [SLOTS] = { false };
uint8_t  rom_image[ROM_SIZE];
uint8_t  alt_rom_image[ALT_ROM_SIZE];

// Resolve the ROM-pointer that should back slot N when NextReg::regs[\$50+N]
// holds 0xFF. NextReg \$8C bit 7 enables the Alt ROM swap for slots 0 and 1
// (Z80 \$0000-\$3FFF). Bit 6 says Alt ROM is only visible to **writes** —
// reads still see the main ROM, so for read-only-mapped slots we keep
// pointing at the main rom_image. Lock bits ($8C bits 5/4 — pin ROM1 / ROM0)
// are TODO; current behaviour treats slot 0 as alt_rom[0..0x1FFF] and slot 1
// as alt_rom[0x2000..0x3FFF].
static inline uint8_t* rom_for_slot(int slot) {
    if (slot < 2) {
        uint8_t altrom = NextReg::regs[0x8C];
        const bool altrom_en        = (altrom & 0x80) != 0;
        const bool altrom_write_only = (altrom & 0x40) != 0;
        if (altrom_en && !altrom_write_only) {
            return alt_rom_image + (slot * SLOT_SIZE);
        }
    }
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
    // Alt ROM mirrors the first 32 KB of main ROM by default — software
    // that flips \$8C bit 7 before NextROMLoader runs sees a coherent
    // sequence rather than 0xFF stream. NextROMLoader::load() rewrites
    // both buffers immediately after init().
    memset(alt_rom_image, 0xFF, sizeof(alt_rom_image));
}

// Re-evaluate slot 0/1 pointers without disturbing the page-number stored
// in NextReg::regs[\$50/\$51]. Used when \$8C changes and ROM-mapped slots
// must swap between main and Alt ROM.
void refresh_rom_slots() {
    for (int slot = 0; slot < 2; ++slot) {
        if (NextReg::regs[0x50 + slot] == PAGE_ROM) {
            slot_ptr[slot] = rom_for_slot(slot);
            slot_ro [slot] = true;
        }
    }
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

    // Re-point the legacy MemESP::ram[0..7] descriptors at NextRAM so any
    // consumer that still reads through the classic 128K view sees the
    // same bytes the MMU exposes. Video.cpp's screen renderer is the
    // critical one — it reads MemESP::ram[5].direct() / ram[7].direct()
    // for the active / shadow screen — but other paths (snapshot save,
    // OSD, debugger) also walk ram[]. Each classic 16K bank covers two
    // consecutive NextRAM 8K pages (bank N → pages 2N, 2N+1, and they're
    // already physically contiguous in NextRAM since it's one flat 2 MB
    // buffer). `locked=true` keeps the descriptor out of the page-swap
    // free list — the buffer is permanently parked in PSRAM.
    if (NextRAM::available) {
        for (int b = 0; b < 8; ++b) {
            MemESP::ram[b].assign_ram(NextRAM::page_ptr(b * 2), b, /*locked=*/true);
        }
    }

    Debug::log("NextMMU: reset — ROM/ROM/RAM5/RAM5/RAM2/RAM2/RAM0/RAM0");
}

} // namespace NextMMU
