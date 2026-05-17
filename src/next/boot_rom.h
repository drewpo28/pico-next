// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: embedded Boot ROM image, mapped into Z80 \$0000-\$3FFF at
// power-on. The CPU starts here, the Boot ROM sets up NextReg state,
// and then writes NextReg \$03 (machine type) which atomically swaps
// itself out — slots 0/1 transition to NextZXOS ROM and execution
// continues from \$0000 of the user-loaded ROM.
//
// Refs:
//   https://wiki.specnext.dev/Next_Register_Internal_Port_0x243B
//   https://wiki.specnext.dev/Configuration_Mode
//   holub/mame src/mame/sinclair/next/specnext.cpp (bootrom_en logic)

#pragma once

#include <stdint.h>

namespace NextBootROM {

// 16 KB Boot ROM image — covers Z80 \$0000-\$3FFF (two 8K MMU slots).
// Same span as the real-hardware Boot ROM. Most of the buffer is 0xFF
// padding; the first dozen bytes are a Z80N stub that hands off to
// NextZXOS by writing 8 to NextReg \$03.
constexpr uint32_t SIZE = 16u * 1024u;

extern const uint8_t image[SIZE];

} // namespace NextBootROM
