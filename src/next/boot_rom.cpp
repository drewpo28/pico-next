// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next embedded Boot ROM — minimal Z80N stub that hands off to
// NextZXOS. The 16 KB buffer occupies Z80 \$0000-\$3FFF while
// NextMMU::bootrom_en is true. After the stub's NEXTREG \$03 write the
// bank_update() path clears bootrom_en and slots 0/1 swap to the
// user-loaded NextZXOS ROM; execution continues at \$0000 of the new
// mapping.
//
// Stub disassembly:
//   0000: F3              DI                 ; disable interrupts
//   0001: 31 FE FF        LD SP, \$FFFE      ; safe stack at end of bank 0
//   0004: 3E 08           LD A, 8            ; machine type 8 = Spectrum Next
//   0006: ED 92 03        NEXTREG \$03, A    ; sets machine + clears bootrom_en
//   0009: C3 00 00        JP \$0000          ; restart in NextZXOS ROM
//
// Total 12 bytes; the remaining 16372 bytes are zero-padded (NOPs). They
// never execute — the unconditional jump at \$0009 redirects to \$0000 of
// the now-active main ROM. If a future Boot ROM grows BIOS-menu / config-
// ini handling it spills into the padding area without changing the
// hand-off entry point.

#include "boot_rom.h"

namespace NextBootROM {

const uint8_t image[SIZE] = {
    0xF3,                          // 0000: DI
    0x31, 0xFE, 0xFF,              // 0001: LD SP, $FFFE
    0x3E, 0x08,                    // 0004: LD A, $08
    0xED, 0x92, 0x03,              // 0006: NEXTREG $03, A
    0xC3, 0x00, 0x00,              // 0009: JP $0000
    // C++ zero-initialises the remainder of the array.
};

} // namespace NextBootROM
