/*

pico-next: ZX Spectrum Next scanline video renderer

*/

#include "NextVideo.h"

#if !PICO_RP2040

#include <string.h>
#include "Video.h"
#include "CPU.h"
#include "ESPectrum.h"
#include "MemESP.h"
#include "NextReg.h"
#include "Debug.h"

// Hot render path in SRAM, like the rest of the video code
#undef IRAM_ATTR
#define IRAM_ATTR __not_in_flash("nextvid")

#pragma GCC optimize("O3")

uint16_t NEXTVID::rawPal[8][256];
uint8_t  NEXTVID::lut[8][256];
uint8_t  NEXTVID::remap[256];

uint8_t NEXTVID::clip[4][4];
uint8_t NEXTVID::clipIdx[4];

uint8_t NEXTVID::palIdx = 0;
bool    NEXTVID::pal9Second = false;
uint8_t NEXTVID::pal9First = 0;
uint8_t NEXTVID::palCtrl = 0;

uint32_t NEXTVID::lineIdx = 0;
uint32_t NEXTVID::totalLines = 311;
uint32_t NEXTVID::nextLineTstate = 228;
int  NEXTVID::firstVisibleLine = 0;
int  NEXTVID::paperTopRow = 0;
int  NEXTVID::xPaperOff = 32;
bool NEXTVID::skipFrame = false;

// ===== palette machinery =====

static inline uint8_t g3r3b2_of_rgb333(uint16_t v) {
    // rgb333 = RRRGGGBBB; CLUT cube layout is GGGRRRBB (grb_to_rgb888)
    uint8_t r = (v >> 6) & 7, g = (v >> 3) & 7, b = v & 7;
    return (g << 5) | (r << 2) | (b >> 1);
}

// Nearest allowed CLUT index for the reserved regions. Indices 0-16 hold the
// Spectrum/OSD colours and 220-255 are used by the scanout (HDMI control
// words, audio data islands, scanline slot) — pixels must not land there.
void NEXTVID::buildRemap() {
    auto allowed = [](int i) { return i >= 17 && i < 220; };
    for (int i = 0; i < 256; i++) {
        if (allowed(i)) { remap[i] = i; continue; }
        int g = (i >> 5) & 7, r = (i >> 2) & 7, b = i & 3;
        int best = 17, bestd = 1 << 30;
        for (int j = 17; j < 220; j++) {
            int jg = (j >> 5) & 7, jr = (j >> 2) & 7, jb = j & 3;
            int dg = g - jg, dr = r - jr, db = b - jb;
            int d = dg * dg * 9 + dr * dr * 9 + db * db * 16; // roughly perceptual
            if (d < bestd) { bestd = d; best = j; }
        }
        remap[i] = best;
    }
}

void NEXTVID::setLutEntry(uint8_t pal, uint8_t idx, uint16_t rgb333) {
    rawPal[pal][idx] = rgb333;
    lut[pal][idx] = remap[g3r3b2_of_rgb333(rgb333)];
}

void NEXTVID::rebuildLut(uint8_t pal) {
    for (int i = 0; i < 256; i++)
        lut[pal][i] = remap[g3r3b2_of_rgb333(rawPal[pal][i])];
}

static inline uint16_t rgb332_to_333(uint8_t v) {
    uint8_t b2 = v & 3;
    return ((v & 0xE0) << 1) | ((v & 0x1C) << 1) | (b2 << 1) | (b2 ? 1 : 0);
}

void NEXTVID::defaultPalettes() {
    // Classic Spectrum colours in RGB333: level 5 normal, 7 bright
    for (int pal = 0; pal < 8; pal++) {
        for (int i = 0; i < 256; i++)
            rawPal[pal][i] = rgb332_to_333((uint8_t)i);
    }
    for (int pal = 0; pal < 8; pal += 4) { // both ULA palettes
        for (int c = 0; c < 16; c++) {
            uint8_t base = c & 7;
            uint8_t l = (c & 8) ? 7 : 5;
            uint16_t v = ((base & 2) ? (l << 6) : 0)   // R
                       | ((base & 4) ? (l << 3) : 0)   // G
                       | ((base & 1) ? l : 0);         // B
            rawPal[pal][c] = v;        // ink
            rawPal[pal][16 + c] = v;   // paper (same colours)
        }
    }
    for (int pal = 0; pal < 8; pal++) rebuildLut(pal);
}

void NEXTVID::palIndex(uint8_t v) {
    palIdx = v;
    pal9Second = false;
}

static inline uint8_t writePalSel(uint8_t ctrl) { return (ctrl >> 4) & 7; }

void NEXTVID::palValue8(uint8_t v) {
    setLutEntry(writePalSel(palCtrl), palIdx, rgb332_to_333(v));
    pal9Second = false;
    if (!(palCtrl & 0x80)) palIdx++;
}

void NEXTVID::palValue9(uint8_t v) {
    if (!pal9Second) {
        pal9First = v;
        pal9Second = true;
    } else {
        // second byte: bit0 = blue LSB (bit7 = Layer2 priority — not yet used)
        uint16_t rgb = (rgb332_to_333(pal9First) & ~1) | (v & 1);
        setLutEntry(writePalSel(palCtrl), palIdx, rgb);
        pal9Second = false;
        if (!(palCtrl & 0x80)) palIdx++;
    }
}

void NEXTVID::palControl(uint8_t v) {
    palCtrl = v;
    pal9Second = false;
}

void NEXTVID::clipWrite(uint8_t win, uint8_t v) {
    win &= 3;
    clip[win][clipIdx[win] & 3] = v;
    clipIdx[win] = (clipIdx[win] + 1) & 3;
}

void NEXTVID::clipIndexReset(uint8_t mask) {
    for (int w = 0; w < 4; w++)
        if (mask & (1 << w)) clipIdx[w] = 0;
}

// ===== geometry / frame control =====

void NEXTVID::Reset() {
    buildRemap();
    defaultPalettes();
    palIdx = 0; pal9Second = false; palCtrl = 0;
    static const uint8_t defclip[4][4] = {
        { 0, 255, 0, 191 },  // Layer 2
        { 0, 255, 0, 191 },  // sprites
        { 0, 255, 0, 191 },  // ULA
        { 0, 159, 0, 255 },  // tilemap
    };
    for (int w = 0; w < 4; w++) {
        for (int i = 0; i < 4; i++) clip[w][i] = defclip[w][i];
        clipIdx[w] = 0;
    }

    totalLines = CPU::statesInFrame ?
        ((CPU::statesInFrame >> ESPectrum::multiplicator) / VIDEO::tStatesPerLine) : 311;
    firstVisibleLine = (VIDEO::tStatesBorder + VIDEO::tStatesPerLine / 2) / VIDEO::tStatesPerLine;
    paperTopRow = (VIDEO::tStatesScreen + VIDEO::tStatesPerLine / 2) / VIDEO::tStatesPerLine
                  - firstVisibleLine;
    xPaperOff = ((int)VIDEO::vga.xres - 256) / 2;
    lineIdx = 0;
    nextLineTstate = (uint32_t)VIDEO::tStatesPerLine << ESPectrum::multiplicator;
    skipFrame = false;
}

IRAM_ATTR void NEXTVID::EndFrame() {
    static uint8_t skipCnt = 0;
    skipFrame = ESPectrum::maxSpeed && (++skipCnt & 63);
    lineIdx = 0;
    nextLineTstate = (uint32_t)VIDEO::tStatesPerLine << ESPectrum::multiplicator;
    totalLines = (CPU::statesInFrame >> ESPectrum::multiplicator) / VIDEO::tStatesPerLine;
}

IRAM_ATTR void NEXTVID::Tick(unsigned int statestoadd, bool contended) {
    CPU::tstates += statestoadd;
    while (CPU::tstates >= nextLineTstate) ScanlineWork();
}

IRAM_ATTR void NEXTVID::Tick_Opcode(bool contended) {
    CPU::tstates += 4;
    while (CPU::tstates >= nextLineTstate) ScanlineWork();
}

void NEXTVID::DrawBorderNop() {}

IRAM_ATTR void NEXTVID::ScanlineWork() {
    uint8_t m = ESPectrum::multiplicator;
    if (lineIdx < totalLines) {
        int row = (int)lineIdx - firstVisibleLine;
        if (!skipFrame && row >= 0 && row < (int)VIDEO::vga.yres)
            RenderLine(row);
        lineIdx++;
        nextLineTstate = (lineIdx + 1) * ((uint32_t)VIDEO::tStatesPerLine << m);
    } else {
        // frame over-run (turbo tail) — wait for EndFrame to restart
        nextLineTstate = 0xFFFFFFFF;
    }
}

// ===== scanline compositor =====

IRAM_ATTR void NEXTVID::RenderLine(int row) {
    // Keep the F8 stats overlay readable: it lives in paper rows 176-191
    // (16:9 modes) or the top of the bottom border (4:3 modes)
    if (VIDEO::OSD && row >= paperTopRow + 176 && row < paperTopRow + 192 + 16)
        return;

    uint8_t* fb = VIDEO::vga.frameBuffer[row];
    int xres = VIDEO::vga.xres;
    const uint8_t* ulaLut = lut[(palCtrl & 0x08) ? 4 : 0];

    uint8_t borderIdx = ulaLut[16 + (VIDEO::borderColor & 7)];
    int y = row - paperTopRow;

    if (y < 0 || y >= 192) {
        memset(fb, borderIdx, xres);
        return;
    }

    // left/right border
    memset(fb, borderIdx, xPaperOff);
    memset(fb + xPaperOff + 256, borderIdx, xres - xPaperOff - 256);

    // ULA layer (classic attribute mode; ULANext later). ULA can be disabled
    // via nextreg 0x68 bit 7 — border colour shows through.
    uint8_t* p = fb + xPaperOff;
    if (!(NextReg::reg[0x68] & 0x80)) {
        const uint8_t* bmp = VIDEO::grmem + VIDEO::offBmp[y];
        const uint8_t* att = VIDEO::grmem + VIDEO::offAtt[y];
        for (int col = 0; col < 32; col++) {
            uint8_t a = att[col];
            uint8_t b = bmp[col] ^ (uint8_t)(-((a & VIDEO::flashing) >> 7));
            uint8_t bright = (a & 0x40) >> 3;
            uint8_t ink = ulaLut[(a & 7) | bright];
            uint8_t pap = ulaLut[16 + (((a >> 3) & 7) | bright)];
            p[0] = (b & 0x80) ? ink : pap;
            p[1] = (b & 0x40) ? ink : pap;
            p[2] = (b & 0x20) ? ink : pap;
            p[3] = (b & 0x10) ? ink : pap;
            p[4] = (b & 0x08) ? ink : pap;
            p[5] = (b & 0x04) ? ink : pap;
            p[6] = (b & 0x02) ? ink : pap;
            p[7] = (b & 0x01) ? ink : pap;
            p += 8;
        }
    } else {
        memset(fb + xPaperOff, borderIdx, 256);
    }

    // Layer 2 (256x192x8bpp), over ULA (default SLU order)
    if ((NextReg::port123B & 0x02) || (NextReg::reg[0x69] & 0x80)) {
        if (y >= clip[0][2] && y <= clip[0][3]) {
            uint16_t sy = y + NextReg::reg[0x17];
            if (sy >= 192) sy -= 192;
            uint16_t page = (uint16_t)(NextReg::reg[0x12] & 0x7F) * 2 + (sy >> 5);
            if (page < MemESP::NEXT_PAGES - 1u) {
                const uint8_t* src = MemESP::nextRamPtr[page] + (sy & 31) * 256;
                uint8_t transp = NextReg::reg[0x14];
                const uint8_t* l2Lut = lut[(palCtrl & 0x04) ? 5 : 1];
                uint8_t sx = NextReg::reg[0x16] + clip[0][0];
                uint8_t* q = fb + xPaperOff + clip[0][0];
                int count = (int)clip[0][1] - (int)clip[0][0] + 1;
                while (count-- > 0) {
                    uint8_t v = src[sx++]; // uint8 wrap = 256px scroll wrap
                    if (v != transp) *q = l2Lut[v];
                    q++;
                }
            }
        }
    }
}

#endif // !PICO_RP2040
