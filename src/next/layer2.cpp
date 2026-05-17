// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: Layer 2 state. See layer2.h. The actual scanline composite
// over the ULA pixels lives in src/Video.cpp where the rendering loop
// holds the line buffer.

#include "layer2.h"

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

} // namespace Layer2
