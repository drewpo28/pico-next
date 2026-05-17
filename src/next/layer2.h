// SPDX-License-Identifier: GPL-3.0-or-later
//
// pico-next: ZX Spectrum Next Layer 2 — independent 8bpp paletted bitmap
// layer overlaid on the ULA / Tilemap / Sprites.
//
// Layer 2 frames live as contiguous pages in Next-RAM. The default mode
// is 256×192 with 1 byte per pixel = 48 KB (3 × 16K banks, six 8K Next
// pages). NextReg $12 holds the start-page index (each unit = 16 KB =
// two 8K pages) of the visible buffer; NextReg $13 is a paging-control
// register that lets software map Layer 2 into the Z80 $0000-$3FFF
// window for fast CPU writes.
//
// This module owns the runtime state Video.cpp's compositor needs:
//   enabled flag (driven by Port $123B bit 1, also reachable via the
//   $123B NextReg write — see Ports.cpp)
//   start page (NextReg $12)
//   resolution mode (NextReg $70)
//   scroll offsets (NextReg $16 X, $17 Y; also $71/$72 for 320/640 modes)
//   priority + clip window (NextReg $15, $18)
//
// Refs: https://wiki.specnext.dev/Layer_2

#pragma once

#include <stdint.h>

namespace Layer2 {

// Mode codes from NextReg $70 bits 5-4.
enum Mode : uint8_t {
    MODE_256x192 = 0,  // 1 byte/pixel, 48 KB framebuffer (default)
    MODE_320x256 = 1,  // 1 byte/pixel, 80 KB framebuffer
    MODE_640x256 = 2,  // 4 bits/pixel, 80 KB framebuffer
};

// Layer 2 visibility — set by Port $123B bit 1 or NextReg-level toggles.
extern bool enabled;

// Start-page index of the visible framebuffer in Next-RAM. Each unit
// represents 16 KB = two 8K Next pages. NextReg $12.
extern uint8_t start_page;

// Paging-control mirror of NextReg $13 (a copy of start_page for the
// shadow Layer-2 area, used during double-buffering).
extern uint8_t start_page_shadow;

// Current display mode, decoded from NextReg $70 bits 5-4.
extern Mode mode;

// Scroll registers, NextReg $16/$17 for 256×192, $71/$72 for 320/640.
extern uint16_t scroll_x;   // 0..639 for 320/640 modes, 0..255 for 256
extern uint8_t  scroll_y;   // 0..255

// Layer 2 palette offset (NextReg $14 bits 7-4 × 16). Adds to each pixel
// index before looking up the Layer-2 palette.
extern uint8_t palette_offset;

// Clip-window coordinates (NextReg $18 — auto-incrementing 4-byte set:
// X1, X2, Y1, Y2). Pixels outside the window aren't drawn.
extern uint8_t clip_x1, clip_x2, clip_y1, clip_y2;

// Internal index for the auto-incrementing $18 writes.
extern uint8_t clip_index;

// Reset Layer 2 state to power-on defaults: disabled, start_page=8 (the
// classic Layer 2 home address), 256×192 mode, no scroll, no palette
// offset, full-screen clip.
void reset();

// Called when NextReg $18 receives a write — advances clip_index 0..3
// across X1, X2, Y1, Y2 in that order.
void writeClip(uint8_t value);

// Reset the clip-write index on NextReg $1C bit-3 trigger (per spec).
void resetClipIndex();

} // namespace Layer2
