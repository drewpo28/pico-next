// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: stub roms.h.
//
// Legacy ESPectrum embedded a Sinclair 48K/128K/Custom/TR-DOS/esxDOS/
// esxIDE ROM set here. pico-next emulates the Spectrum Next exclusively,
// and its ROM is loaded from /machines/next/enNxtmmc.rom (or enNextZX.rom)
// on the SD card at boot (see src/next/rom_loader.cpp). Consequently no
// embedded ROM symbols are exported from here anymore — but the header
// still exists because half a dozen translation units #include "roms.h"
// and removing the includes is mechanical noise unrelated to ROM cleanup.

#ifndef ROMS_H
#define ROMS_H

#endif // ROMS_H
