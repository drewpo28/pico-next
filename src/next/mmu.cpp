// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: 8K MMU implementation. See mmu.h.

#include "mmu.h"

#include <cstring>

#include "psram_ram.h"
#include "nextreg.h"
#include "boot_rom.h"
#include "../MemESP.h"
#include "../Debug.h"

namespace NextMMU {

uint8_t* slot_ptr[SLOTS] = { nullptr };
bool     slot_ro [SLOTS] = { false };
uint8_t  rom_image[ROM_SIZE];
uint8_t  alt_rom_image[ALT_ROM_SIZE];
uint8_t  rom_bank  = 0;
bool     bootrom_en = true;

// Pick the right 8 KB ROM window for slot N (0..1) given the current
// rom_bank, bootrom_en and \$8C state. Priority order:
//   1. Boot ROM (bootrom_en true — embedded stub, slots 0-1 only)
//   2. Alt ROM with lock-bit pin (NextReg \$8C bits 4/5)
//   3. Alt ROM with rom_bank-derived window (NextReg \$8C bit 7)
//   4. Main ROM with rom_bank-derived 16 KB window
// \$8C bit 6 ("Alt ROM only on writes") means reads still see main ROM —
// pico-next is read-mostly here so writes to ROM slots are dropped by
// slot_ro either way.
static inline uint8_t* rom_for_slot(int slot) {
    if (slot < 2 && bootrom_en) {
        // Boot ROM occupies the same 16 KB as the main ROM bank 0 — slot
        // 0 reads its low 8 KB, slot 1 reads its high 8 KB. After the
        // embedded stub writes NextReg \$03 bootrom_en clears and a
        // subsequent bank_update() returns to the normal path below.
        return const_cast<uint8_t*>(NextBootROM::image + (slot * SLOT_SIZE));
    }
    if (slot < 2) {
        const uint8_t altrom = NextReg::regs[0x8C];
        const bool altrom_en         = (altrom & 0x80) != 0;
        const bool altrom_write_only = (altrom & 0x40) != 0;
        const bool lock_rom0         = (altrom & 0x10) != 0;  // pin Alt ROM bank 0 (128K)
        const bool lock_rom1         = (altrom & 0x20) != 0;  // pin Alt ROM bank 1 (48K)
        if (altrom_en && !altrom_write_only) {
            // Alt ROM is 32 KB = 2 × 16 KB banks. Lock bits override the
            // rom_bank window: bit 4 forces bank 0, bit 5 forces bank 1.
            // Both bits set is documented as "lock to bank 0" on real
            // hardware (bit 4 wins) — match that.
            uint32_t bank16;
            if (lock_rom0)      bank16 = 0;
            else if (lock_rom1) bank16 = 0x4000;
            else                bank16 = ((uint32_t)rom_bank & 0x01) * 0x4000;
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

// +3 special all-RAM paging table. \$1FFD bit 0 = 1 enters this mode; bits
// 2:1 then select one of four classic 16K-bank layouts (no ROM in the
// map). Each entry is the legacy RAM bank for that 16K slot — translate
// to Next 8K page by `bank * 2 + low_half`.
//   config = ($1FFD >> 1) & 3
//   legacy_slot_index 0..3 = Z80 \$0000 / \$4000 / \$8000 / \$C000
//   bank = special_banks[config][legacy_slot_index]
// Ref: Spectrum +3 technical manual; mirrored on Next core 3.x.
static const uint8_t special_banks[4][4] = {
    {0, 1, 2, 3},
    {4, 5, 6, 7},
    {4, 5, 6, 3},
    {4, 7, 6, 3},
};

static inline bool special_paging_active() {
    return (MemESP::port_1ffd_data & 0x01) != 0;
}

void bank_update(int slot) {
    if (slot < 0 || slot >= SLOTS) return;

    // Priority 1: +3 special paging mode — all-RAM, ignores NextReg
    // \$50-\$57 and Alt ROM. Bank-per-16K-slot from the lookup table.
    if (special_paging_active() && NextRAM::available) {
        const uint8_t config       = (MemESP::port_1ffd_data >> 1) & 0x03;
        const int     legacy_slot  = slot >> 1;
        const int     legacy_half  = slot & 0x01;
        const uint8_t bank         = special_banks[config][legacy_slot];
        const uint32_t page        = (uint32_t)bank * 2u + (uint32_t)legacy_half;
        slot_ptr[slot] = NextRAM::page_ptr(page);
        slot_ro [slot] = false;
        return;
    }

    const uint8_t page = NextReg::regs[0x50 + slot];

    // Priority 2: ROM page (0xFF). rom_for_slot() handles Alt ROM, lock
    // bits and rom_bank-derived window inside.
    if (page == PAGE_ROM) {
        slot_ptr[slot] = rom_for_slot(slot);
        slot_ro [slot] = true;
        return;
    }

    // Priority 3: documented Next-RAM page 0..223.
    if (page < NextRAM::PAGE_COUNT && NextRAM::available) {
        slot_ptr[slot] = NextRAM::page_ptr(page);
        slot_ro [slot] = false;
        return;
    }

    // Priority 4: unmapped fallback — point at main ROM so the bus reads
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

    // \$1FFD bit 0 toggles +3 all-RAM paging which affects every slot,
    // not just 0/1. Track that separately so we can issue a full rebuild
    // when special mode enters or leaves (or when the config bits 2/1
    // change while in special mode).
    static uint8_t last_1ffd = 0;
    const uint8_t  this_1ffd = MemESP::port_1ffd_data;
    const bool was_special   = (last_1ffd & 0x01) != 0;
    const bool now_special   = (this_1ffd & 0x01) != 0;
    const bool special_layout_changed =
        was_special != now_special ||
        (now_special && (last_1ffd & 0x06) != (this_1ffd & 0x06));

    if (special_layout_changed) {
        last_1ffd = this_1ffd;
        rom_bank = now;
        bank_update_all();
        return;
    }
    last_1ffd = this_1ffd;

    if (now != rom_bank) {
        rom_bank = now;
        // Only slot 0/1 source from main ROM with the bank offset; the
        // other six slots don't need a refresh in the common case.
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

    // Re-engage the Boot ROM on every reset — the embedded stub at
    // NextBootROM::image hands off via NextReg \$03 each time, giving a
    // deterministic power-on sequence. NextZXOS doesn't care whether
    // this is a cold boot or an F11 reset since both go through Boot
    // ROM first.
    bootrom_en = true;

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
