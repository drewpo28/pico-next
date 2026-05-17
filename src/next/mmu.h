// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: ZX Spectrum Next MMU (8 KB-page Z80 memory map).
//
// The Spectrum Next divides the Z80 64 KB address space into eight 8 KB
// slots. Each slot's source page is selected by NextReg $50-$57 (one
// register per slot, value = physical 8K page index 0-223 for RAM, or
// 0xFF for ROM). This module maintains a fast pointer cache so byte
// access from the Z80 hot path is one indexed lookup.
//
// Refs:
//   https://wiki.specnext.dev/Memory_map
//   https://wiki.specnext.dev/Memory_Mapping_Register

#pragma once

#include <stdint.h>

namespace NextMMU {

constexpr uint32_t SLOT_SIZE = 8u * 1024u;
constexpr uint32_t SLOT_MASK = SLOT_SIZE - 1u;
constexpr uint32_t SLOT_SHIFT = 13u;
constexpr int      SLOTS = 8;

// Page number that NextReg $50+slot resolves to "ROM" rather than RAM.
constexpr uint8_t  PAGE_ROM = 0xFF;

// Per-slot host pointer that the Z80 bus reads/writes through. Each entry
// points at an 8 KB region: either Next-RAM (NextRAM::page_ptr(n)) or the
// ROM placeholder buffer. Public for cache-friendly inline access from
// MemESP/Z80Ops.
extern uint8_t* slot_ptr[SLOTS];

// Per-slot write protection. ROM-mapped slots have this set so writebyte()
// silently drops writes instead of corrupting the ROM placeholder.
extern bool     slot_ro[SLOTS];

// One-time initialisation: fill the ROM placeholder with 0xFF (so reads
// before the SD ROM-loader populates rom_image[] decode to RST $38 and
// show up clearly in traces). Called once from ESPectrum::setup(); a CPU
// reset (F11) must not wipe ROM, so this is split from reset().
void init();

// Re-seed the MMU slot pointers to the post-power-on defaults documented
// by the Memory Mapping Register page (ROM in slots 0-1, RAM bank 5 in
// slots 2-3, RAM bank 2 in slots 4-5, RAM bank 0 in slots 6-7). Called
// from ESPectrum::setup()/reset() before the CPU fetches its first
// opcode. Does **not** touch rom_image[] — use init() for that.
void reset();

// Re-evaluate slot 0/1 ROM mapping after NextReg \$8C changes. Doesn't
// touch RAM-mapped slots or other ROM-page-related state.
void refresh_rom_slots();

// Update slot N (0..7) to source from physical Next page number.
// page < 224     → NextRAM page
// page == 0xFF   → ROM (slot becomes read-only, points at ROM buffer)
// otherwise      → treated as ROM for now (matches Next "reserved"
//                  semantics; future revisions can route 0xE0-0xFE to
//                  extra-ROM or registered devices).
// Called from NextReg::write() when the register written is in $50..$57.
void set_slot(int slot, uint8_t page);

// Z80-bus byte read/write. addr is a 16-bit Z80 address; the upper three
// bits select the slot. Inline for the hot path.
inline uint8_t readbyte(uint16_t addr) {
    return slot_ptr[addr >> SLOT_SHIFT][addr & SLOT_MASK];
}

inline void writebyte(uint16_t addr, uint8_t value) {
    const int slot = addr >> SLOT_SHIFT;
    if (slot_ro[slot]) return;
    slot_ptr[slot][addr & SLOT_MASK] = value;
}

// 64 KB of ROM placeholder space (8 pages × 8 KB). NextROMLoader populates
// this from enNextZX.rom / enNxtmmc.rom at boot. Until then it reads as
// 0xFF — i.e. RST $38 on the bus, exposing any MMU misrouting as a
// visible PC=$0038 loop.
constexpr uint32_t ROM_SIZE = 64u * 1024u;
extern uint8_t rom_image[ROM_SIZE];

// 32 KB of Alt ROM. NextReg \$8C bit 7 enables Alt ROM, which on real
// hardware swaps in this region instead of the main ROM at Z80 \$0000-
// \$3FFF for read accesses (and optionally write accesses too — \$8C
// bit 6). Some firmware ships a separate enAltZX.rom; until pico-next
// learns to load it, we mirror the first 32 KB of the main rom_image
// into this buffer at init so software that flips \$8C bit 7 sees
// consistent code instead of 0xFF bus garbage.
constexpr uint32_t ALT_ROM_SIZE = 32u * 1024u;
extern uint8_t alt_rom_image[ALT_ROM_SIZE];

} // namespace NextMMU
