// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: 2 MB Spectrum Next RAM in butter PSRAM.
//
// Spectrum Next has 2 MB of RAM, physically organised as 224 + 32 = 256
// 8 KB pages (the upper 32 pages mirror Alt-ROM / reserved areas). The
// MMU ($50-$57 / src/next/mmu) maps any of these into one of the eight
// 8 KB Z80 address-space slots.
//
// This module owns the linear 2 MB storage region. It lives at the top
// of the RP2350B onboard butter PSRAM so the bottom of butter PSRAM
// stays free for the existing legacy Spectrum bank allocator in
// ESPectrum::assign_ram() (compat-mode boots don't touch Next-RAM, and
// Next-mode boots don't touch the legacy bank slots).
//
// Ref: https://wiki.specnext.dev/Memory_map

#pragma once

#include <stdint.h>

namespace NextRAM {

constexpr uint32_t SIZE       = 2u * 1024u * 1024u;     // 2 MB physical RAM
constexpr uint32_t PAGE_SIZE  = 8u * 1024u;             // 8 KB pages (Next MMU)
constexpr uint32_t PAGE_COUNT = SIZE / PAGE_SIZE;       // 256

// Start of the 2 MB region in butter PSRAM; nullptr if init() failed.
extern uint8_t* base;

// True after successful init(). Consumers (MMU, ROM loader, ...) gate on
// this and downgrade Next-mode to an error if false.
extern bool available;

// Allocate the 2 MB region at the top of butter PSRAM and zero it.
// Idempotent — subsequent calls return the cached state without re-zeroing.
// Returns true on success, false if butter PSRAM is smaller than 2 MB.
bool init();

inline uint8_t* page_ptr(uint32_t page_index) {
    return base + (page_index * PAGE_SIZE);
}

inline uint8_t  read(uint32_t paddr) { return base[paddr]; }
inline void     write(uint32_t paddr, uint8_t v) { base[paddr] = v; }

} // namespace NextRAM
