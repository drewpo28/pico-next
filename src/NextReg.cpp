/*

pico-next: ZX Spectrum Next (TBBlue) NextReg register file

*/

#include "NextReg.h"

#if !PICO_RP2040

#include "Debug.h"

#pragma GCC optimize("O3")

uint8_t NextReg::reg[256];
uint8_t NextReg::selected = 0;

void NextReg::reset(bool hard) {
    if (hard) {
        for (int i = 0; i < 256; i++) reg[i] = 0;
        selected = 0;
    }
    // MMU defaults: ROM in slots 0/1, banks 5,2,0 in RAM slots
    reg[0x50] = 0xFF; reg[0x51] = 0xFF;
    reg[0x52] = 10;   reg[0x53] = 11;
    reg[0x54] = 4;    reg[0x55] = 5;
    reg[0x56] = 0;    reg[0x57] = 1;
    reg[0x07] = 0;    // 3.5 MHz
}

void NextReg::write(uint8_t r, uint8_t v) {
    // Register side effects are wired up incrementally; unknown registers
    // are latched so software that reads them back keeps working.
    reg[r] = v;
}

uint8_t NextReg::read(uint8_t r) {
    switch (r) {
        case 0x00: return 10;    // machine ID: ZX Spectrum Next
        case 0x01: return 0x32;  // core major.minor = 3.2
        case 0x0E: return 0x00;  // core sub-minor
        default:   return reg[r];
    }
}

#endif // !PICO_RP2040
