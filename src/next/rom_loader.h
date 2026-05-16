// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: load NextZXOS ROM image from SD into NextMMU::rom_image.
//
// Real Spectrum Next FPGAs read /machines/next/enNextZX.rom (or the
// MMC-capable variant enNxtmmc.rom) into Alt-ROM at power-up. We mirror
// the same lookup so a user can drop the official "System/Next" distro
// onto an SD card and boot it unchanged.
//
// Refs:
//   https://wiki.specnext.dev/NextZXOS
//   https://wiki.specnext.dev/Distro_files

#pragma once

#include <stdint.h>

namespace NextROMLoader {

// Try the standard distro paths in order and copy the first one that
// opens into NextMMU::rom_image[]. Returns true on success. Logs the
// chosen path, file size, and a header dump to UART for diagnostics.
//
// Lookup order:
//   /machines/next/enNxtmmc.rom    (preferred — esxDOS/MMC ROM)
//   /machines/next/enNextZX.rom    (vanilla Next ROM)
//   /enNxtmmc.rom                  (root fallback for dev setups)
//   /enNextZX.rom                  (root fallback for dev setups)
bool load();

// True once load() succeeded at least once this boot.
extern bool loaded;

} // namespace NextROMLoader
