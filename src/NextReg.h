/*

pico-next: ZX Spectrum Next (TBBlue) NextReg register file

Access from Z80 via port 0x243B (select) / 0x253B (data) and the Z80N
NEXTREG opcodes (ED 91 / ED 92). The Copper writes through the same
dispatcher.

Reference: https://wiki.specnext.dev/TBBlue_Register_Select

*/

#ifndef NextReg_h
#define NextReg_h

#if !PICO_RP2040

#include <inttypes.h>

class NextReg {
public:
    static uint8_t reg[256];     // raw latched values
    static uint8_t selected;     // port 0x243B latch

    static void reset(bool hard);
    static void write(uint8_t r, uint8_t v);
    static uint8_t read(uint8_t r);
};

#endif // !PICO_RP2040

#endif // NextReg_h
