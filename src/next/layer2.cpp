// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: Layer 2 state + 256×192 compositor.
// See layer2.h for module overview.

#include "layer2.h"

#include "psram_ram.h"
#include "nextreg.h"
#include "../Video.h"

namespace Layer2 {

bool     enabled            = false;
uint8_t  start_page         = 8;    // power-on default per the wiki
uint8_t  start_page_shadow  = 11;   // shadow buffer two banks higher
Mode     mode               = MODE_256x192;
uint16_t scroll_x           = 0;
uint8_t  scroll_y           = 0;
uint8_t  palette_offset     = 0;
uint8_t  clip_x1            = 0;
uint8_t  clip_x2            = 255;  // 256×192 default — full frame
uint8_t  clip_y1            = 0;
uint8_t  clip_y2            = 191;
uint8_t  clip_index         = 0;

void reset() {
    enabled           = false;
    start_page        = 8;
    start_page_shadow = 11;
    mode              = MODE_256x192;
    scroll_x          = 0;
    scroll_y          = 0;
    palette_offset    = 0;
    clip_x1           = 0;
    clip_x2           = 255;
    clip_y1           = 0;
    clip_y2           = 191;
    clip_index        = 0;
}

void writeClip(uint8_t value) {
    // NextReg $18 sequence: X1, X2, Y1, Y2, wrapping back to X1.
    switch (clip_index & 0x03) {
        case 0: clip_x1 = value; break;
        case 1: clip_x2 = value; break;
        case 2: clip_y1 = value; break;
        case 3: clip_y2 = value; break;
    }
    clip_index = (uint8_t)((clip_index + 1) & 0x03);
}

void resetClipIndex() {
    clip_index = 0;
}

// HW palette layout in pico-next while Layer 2 is on:
//   slots 0..15   ULA standard colours (16-colour mode)
//   slots 16..255 Layer 2 first-palette (NextReg::palette[1])
// We have 240 host slots for Layer 2's 256 colours, so palette indices
// 240..255 alias modulo 240 — splash screens use the low end of the
// palette and don't notice, but software pushing the upper end will
// see colour collisions. A second-palette page-flip and a dynamic
// allocator can fix this once we need it.
constexpr int L2_HW_BASE = 16;
constexpr int L2_HW_SLOTS = 240;

void composite() {
    if (!enabled)                 return;
    if (mode != MODE_256x192)     return;   // 320×256 / 640×256 — TODO
    if (!NextRAM::available)      return;
    if (!VIDEO::vga.frameBuffer)  return;

    const int FB_W = (int)VIDEO::vga.xres;
    const int FB_H = (int)VIDEO::vga.yres;
    if (FB_W < 256 || FB_H < 192) return;

    // Centre 256×192 in the host framebuffer (typical 320×240 VGA mode
    // gives 32 px left/right + 24 px top/bottom border).
    const int xoff = (FB_W - 256) / 2;
    const int yoff = (FB_H - 192) / 2;

    // Bounds-check the framebuffer offset. start_page is 8-bit so a write
    // of 0x80+ would aim past the 2 MB NextRAM region. Skip the composite
    // rather than read garbage; in practice firmware keeps it in the
    // documented 0x08-0x0E range for 256×192 / 8-12 for shadow buffer.
    const uint32_t fb_offset = (uint32_t)start_page * 16384u;
    if (fb_offset + 256u * 192u > NextRAM::SIZE) return;

    const uint8_t* l2 = NextRAM::base + fb_offset;
    const uint8_t  pal_offset = palette_offset;  // pre-masked 0xF0 on $14 write

    for (int y = 0; y < 192; ++y) {
        uint8_t* dst       = (uint8_t*)VIDEO::vga.frameBuffer[yoff + y] + xoff;
        const uint8_t* src = l2 + y * 256;
        for (int x = 0; x < 256; ++x) {
            const uint8_t pix = src[x];
            // Index 0 is the conventional transparent colour for Layer 2;
            // leave the ULA pixel underneath alone.
            if (pix == 0) continue;
            const uint8_t idx = (uint8_t)(pix + pal_offset);
            dst[x] = (uint8_t)(L2_HW_BASE + (idx % L2_HW_SLOTS));
        }
    }
}

} // namespace Layer2
