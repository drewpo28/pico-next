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
uint8_t  rom_bank = 0;

// Pick the right 8 KB ROM window for slot N (0..1) given the current
// rom_bank and \$8C state. Priority order:
//   1. Alt ROM (NextReg \$8C bit 7 set, bit 6 clear)
//   2. Main ROM with rom_bank-derived 16 KB window
// Lock bits \$8C bits 5/4 (pin ROM1 / ROM0) and the Alt ROM "writes only"
// bit (bit 6) are still TODO — until then bit 6 means "reads stay on main
// ROM" and locks are ignored.
static inline uint8_t* rom_for_slot(int slot) {
    if (slot < 2) {
        const uint8_t altrom = NextReg::regs[0x8C];
        const bool altrom_en         = (altrom & 0x80) != 0;
        const bool altrom_write_only = (altrom & 0x40) != 0;
        if (altrom_en && !altrom_write_only) {
            // Alt ROM is 32 KB = 2 × 16 KB banks. Bit 5/4 (lock ROM1 / ROM0)
            // would pin one of those; current behaviour picks the same
            // 16 KB window as main ROM but capped to Alt ROM's range.
            const uint32_t bank16 = ((uint32_t)rom_bank & 0x01) * 0x4000;
            return alt_rom_image + bank16 + (slot * SLOT_SIZE);
        }
    }
    // Main ROM is 64 KB = 4 × 16 KB banks. rom_bank selects which 16 KB
    // covers slots 0/1; other slots address into rom_image directly when
    // they happen to be ROM-mapped (rare; ROM at slot 2-7 was used by
    // some test ROMs).
    if (slot < 2) {
        const uint32_t bank16 = (uint32_t)rom_bank * 0x4000;
        return rom_image + bank16 + (slot * SLOT_SIZE);
    }
    return rom_image + (slot * SLOT_SIZE);
}

void bank_update(int slot) {
    if (slot < 0 || slot >= SLOTS) return;
    const uint8_t page = NextReg::regs[0x50 + slot];

    // Priority 1: ROM page (0xFF). rom_for_slot() handles Alt ROM and
    // rom_bank-derived window inside.
    if (page == PAGE_ROM) {
        slot_ptr[slot] = rom_for_slot(slot);
        slot_ro [slot] = true;
        return;
    }

    // Priority 2: documented Next-RAM page 0..223.
    if (page < NextRAM::PAGE_COUNT && NextRAM::available) {
        slot_ptr[slot] = NextRAM::page_ptr(page);
        slot_ro [slot] = false;
        return;
    }

    // Priority 3: unmapped fallback — point at main ROM so the bus reads
    // as 0xFF and the CPU loops at RST $38, exposing the misroute in the
    // UART trace instead of silently dereferencing a stale pointer.
    slot_ptr[slot] = rom_for_slot(slot);
    slot_ro [slot] = true;
}

void bank_update_all() {
    for (int i = 0; i < SLOTS; ++i) bank_update(i);
}

void update_rom_bank() {
    // ROM bank index combines $7FFD bit 4 (LSB) with $1FFD bit 2 (MSB)
    // for a 2-bit 0..3 selector. MemESP::romLatch tracks the former
    // (Ports.cpp pulls it out of every $7FFD write); port_1ffd_data is
    // raw last-write to $1FFD.
    const uint8_t low  = (MemESP::romLatch & 0x01);
    const uint8_t high = (MemESP::port_1ffd_data >> 2) & 0x01;
    const uint8_t now  = (uint8_t)(low | (high << 1));
    if (now != rom_bank) {
        rom_bank = now;
        // Only slot 0/1 source from main ROM with the bank offset, so the
        // other six slots don't need a refresh.
        bank_update(0);
        bank_update(1);
    }
}

void set_slot(int slot, uint8_t page) {
    if (slot < 0 || slot >= SLOTS) return;
    NextReg::regs[0x50 + slot] = page;
    bank_update(slot);
}

void refresh_rom_slots() {
    bank_update(0);
    bank_update(1);
}

void init() {
    // Fill the ROM placeholder so unmapped fetches decode to RST $38.
    memset(rom_image, 0xFF, sizeof(rom_image));
    // Alt ROM mirrors the first 32 KB of main ROM by default — software
    // that flips \$8C bit 7 before NextROMLoader runs sees a coherent
    // sequence rather than 0xFF stream. NextROMLoader::load() rewrites
    // both buffers immediately after init().
    memset(alt_rom_image, 0xFF, sizeof(alt_rom_image));
    rom_bank = 0;
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

    // Soft reset clears the ROM bank index back to 0 (bank 0 = main BASIC
    // / NextZXOS boot code at \$0000-\$3FFF). \$7FFD bit 4 and \$1FFD bit 2
    // are also reset by Ports.cpp via the usual reset path; this guards
    // against ordering issues if MemESP::romLatch hasn't been re-zeroed
    // yet when bank_update() runs.
    rom_bank = 0;

    // Mirror the defaults into NextReg $50-$57 so software-visible state
    // matches what bank_update() installs.
    for (int i = 0; i < SLOTS; ++i) {
        NextReg::regs[0x50 + i] = defaults[i];
        bank_update(i);
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
