// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: Next-RAM allocation in butter PSRAM. See psram_ram.h.

#include "psram_ram.h"

#include <cstring>

#include "../MemESP.h"     // PSRAM_DATA, butter_psram_size()
#include "../Debug.h"

namespace NextRAM {

uint8_t* base      = nullptr;
bool     available = false;

bool init() {
    if (available) return true;

    const uint32_t psize = butter_psram_size();
    if (psize < SIZE) {
        Debug::log("NextRAM: butter PSRAM too small (%u bytes < required %u)",
                   (unsigned)psize, (unsigned)SIZE);
        base = nullptr;
        available = false;
        return false;
    }

    // Park Next-RAM at the top of butter PSRAM. The legacy bank allocator
    // in ESPectrum::assign_ram() fills butter PSRAM from offset 0 upward
    // and DivMMC's optional butter-PSRAM bank cache follows it. On a
    // Pimoroni Pico Plus 2 (8 MB butter) we sit at PSRAM_DATA + 6 MB,
    // leaving 6 MB for legacy use.
    base = (uint8_t*)PSRAM_DATA + (psize - SIZE);

    // NextZXOS expects RAM to be zero at power-on; many of its boot-time
    // sanity checks rely on this. ~150 ms one-shot cost at PSRAM speeds.
    memset(base, 0, SIZE);

    available = true;
    Debug::log("NextRAM: 2 MB mapped at %p (butter offset 0x%X)",
               base, (unsigned)(psize - SIZE));
    return true;
}

} // namespace NextRAM
