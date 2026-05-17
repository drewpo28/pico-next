// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: SD-side ROM loader for NextZXOS. See rom_loader.h.

#include "rom_loader.h"

#include <cstring>

#include "ff.h"

#include "mmu.h"
#include "../FileUtils.h"
#include "../Debug.h"

namespace NextROMLoader {

bool loaded = false;

static const char* CANDIDATE_PATHS[] = {
    "/machines/next/enNxtmmc.rom",
    "/machines/next/enNextZX.rom",
    "/enNxtmmc.rom",
    "/enNextZX.rom",
};

static bool try_path(const char* path) {
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK) return false;

    FSIZE_t fsz = f_size(&fil);
    if (fsz == 0) {
        Debug::log("NextROM: %s is empty, skipping", path);
        f_close(&fil);
        return false;
    }
    // Clamp to placeholder capacity. enNextZX.rom is canonically 64 KB
    // (4 × 16 KB banks) and matches NextMMU::ROM_SIZE exactly. If a future
    // distro grows past that, drop the excess and log so we notice.
    size_t to_read = (fsz > NextMMU::ROM_SIZE) ? NextMMU::ROM_SIZE : (size_t)fsz;
    if (fsz > NextMMU::ROM_SIZE) {
        Debug::log("NextROM: %s is %u bytes, truncating to %u",
                   path, (unsigned)fsz, (unsigned)NextMMU::ROM_SIZE);
    }

    UINT br = 0;
    FRESULT res = f_read(&fil, NextMMU::rom_image, to_read, &br);
    f_close(&fil);
    if (res != FR_OK || br != to_read) {
        Debug::log("NextROM: read %s failed res=%d br=%u/%u",
                   path, (int)res, (unsigned)br, (unsigned)to_read);
        return false;
    }

    // If the file was shorter than the buffer, fill the rest with 0xFF so
    // an over-read still decodes to RST $38 instead of pointing at the
    // tail of whatever ROM image we last loaded.
    if (to_read < NextMMU::ROM_SIZE) {
        memset(NextMMU::rom_image + to_read, 0xFF, NextMMU::ROM_SIZE - to_read);
    }

    // Alt ROM (32 KB) defaults to a copy of the first half of the main
    // ROM. NextZXOS that flips NextReg \$8C bit 7 then sees coherent code
    // instead of 0xFF. Future enhancement: try loading enAltZX.rom from
    // SD and overlay it on top of this mirror.
    memcpy(NextMMU::alt_rom_image, NextMMU::rom_image, NextMMU::ALT_ROM_SIZE);

    Debug::log("NextROM: loaded %s (%u bytes) head=%02X %02X %02X %02X",
               path, (unsigned)br,
               NextMMU::rom_image[0], NextMMU::rom_image[1],
               NextMMU::rom_image[2], NextMMU::rom_image[3]);
    return true;
}

bool load() {
    if (!FileUtils::fsMount) {
        Debug::log("NextROM: SD not mounted, leaving placeholder ROM (0xFF)");
        return false;
    }

    const int n = (int)(sizeof(CANDIDATE_PATHS) / sizeof(CANDIDATE_PATHS[0]));
    for (int i = 0; i < n; ++i) {
        if (try_path(CANDIDATE_PATHS[i])) {
            loaded = true;
            return true;
        }
    }
    Debug::log("NextROM: no enNextZX.rom or enNxtmmc.rom found; tried %d paths", n);
    return false;
}

} // namespace NextROMLoader
