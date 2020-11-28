// Bus Raider
// Rob Dobson 2019

#include "TargetCPUZ80.h"
#include <string.h>
#include <stdlib.h>
#include "lowlev.h"

// Module name
static const char MODULE_PREFIX[] = "TargetCPUZ80";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utils
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetCPUZ80::store16BitVal(uint8_t arry[], int offset, uint16_t val)
{
    arry[offset] = val & 0xff;
    arry[offset+1] = (val >> 8) & 0xff;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle register setting when injecting opcodes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t TargetCPUZ80::getInjectToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen)
{
    // Instructions to set register values
    static uint8_t regSetInstructions[] = 
    {
        0x00,                       // nop - in case previous instruction was prefixed
        0xdd, 0x21, 0x00, 0x00,     // ld ix, xxxx
        0xfd, 0x21, 0x00, 0x00,     // ld iy, xxxx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xd9,                       // exx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x08,                       // ex af,af'
        0xf1, 0x00, 0x00,           // pop af + two bytes that are read as if from stack
        0x31, 0x00, 0x00,           // ld sp, xxxx
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x47,                 // ld i, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x4f,                 // ld r, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x46,                 // im 0
        0xfb,                       // ei
        0xc3, 0x00, 0x00            // jp xxxx
    };
    const int RegisterIXUpdatePos = 3;
    const int RegisterIYUpdatePos = 7;
    const int RegisterHLDASHUpdatePos = 10;
    const int RegisterDEDASHUpdatePos = 13;
    const int RegisterBCDASHUpdatePos = 16;
    const int RegisterHLUpdatePos = 20;
    const int RegisterDEUpdatePos = 23;
    const int RegisterBCUpdatePos = 26;
    const int RegisterAFDASHUpdatePos = 29;
    const int RegisterAFUpdatePos = 33;
    const int RegisterSPUpdatePos = 36;
    const int RegisterIUpdatePos = 39;
    const int RegisterRUpdatePos = 43;
    const int RegisterAUpdatePos = 47;
    const int RegisterIMUpdatePos = 49;
    const int RegisterINTENUpdatePos = 50;
    const int RegisterPCUpdatePos = 52;

    // Fill in the register values
    store16BitVal(regSetInstructions, RegisterIXUpdatePos, regs.IX);
    store16BitVal(regSetInstructions, RegisterIYUpdatePos, regs.IY);
    store16BitVal(regSetInstructions, RegisterHLDASHUpdatePos, regs.HLDASH);
    store16BitVal(regSetInstructions, RegisterDEDASHUpdatePos, regs.DEDASH);
    store16BitVal(regSetInstructions, RegisterBCDASHUpdatePos, regs.BCDASH);
    store16BitVal(regSetInstructions, RegisterSPUpdatePos, regs.SP);
    store16BitVal(regSetInstructions, RegisterHLUpdatePos, regs.HL);
    store16BitVal(regSetInstructions, RegisterDEUpdatePos, regs.DE);
    store16BitVal(regSetInstructions, RegisterBCUpdatePos, regs.BC);
    store16BitVal(regSetInstructions, RegisterAFDASHUpdatePos, regs.AFDASH);
    store16BitVal(regSetInstructions, RegisterAFUpdatePos, regs.AF);
    regSetInstructions[RegisterIUpdatePos] = regs.I;
    regSetInstructions[RegisterRUpdatePos] = (regs.R + 256 - 5) % 256;
    regSetInstructions[RegisterAUpdatePos] = regs.AF >> 8;
    regSetInstructions[RegisterIMUpdatePos] = (regs.INTMODE == 0) ? 0x46 : ((regs.INTMODE == 1) ? 0x56 : 0x5e);
    regSetInstructions[RegisterINTENUpdatePos] = (regs.INTENABLED == 0) ? 0xf3 : 0xfb;
    store16BitVal(regSetInstructions, RegisterPCUpdatePos, regs.PC);

    if (codeMaxlen >= sizeof(regSetInstructions))
    {
        memcopyfast(pCodeBuffer, regSetInstructions, sizeof(regSetInstructions));
        return sizeof(regSetInstructions);
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle snippet to set regs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t TargetCPUZ80::getSnippetToSetRegs(uint32_t codeLocation, Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen)
{
    // Instructions to set register values
    static uint8_t regSetInstructions[] = 
    {
        0x31, 0x00, 0x00,           // ld sp, xxxx
        0x00,                       // nop
        0x18, 0x04,                 // jp xxxx
        0x00, 0x00,                 // af value to be fixed up
        0x00, 0x00,                 // afdash value to be fixed up
        0xdd, 0x21, 0x00, 0x00,     // ld ix, xxxx
        0xfd, 0x21, 0x00, 0x00,     // ld iy, xxxx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xd9,                       // exx
        0x21, 0x00, 0x00,           // ld hl, xxxx
        0x11, 0x00, 0x00,           // ld de, xxxx
        0x01, 0x00, 0x00,           // ld bc, xxxx
        0xf1,                       // pop af
        0x08,                       // ex af,af'
        0xf1,                       // pop af
        0x31, 0x00, 0x00,           // ld sp, xxxx
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x47,                 // ld i, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x4f,                 // ld r, a
        0x3e, 0x00,                 // ld a, xx
        0xed, 0x46,                 // im 0
        0xfb,                       // ei
        0xc3, 0x00, 0x00            // jp xxxx
    };
    const int TempSPUpdatePos = 1;
    const int RegisterAFDASHUpdatePos = 6;
    const int RegisterAFUpdatePos = 8;
    const int RegisterIXUpdatePos = 12;
    const int RegisterIYUpdatePos = 16;
    const int RegisterHLDASHUpdatePos = 19;
    const int RegisterDEDASHUpdatePos = 22;
    const int RegisterBCDASHUpdatePos = 25;
    const int RegisterHLUpdatePos = 29;
    const int RegisterDEUpdatePos = 32;
    const int RegisterBCUpdatePos = 35;
    const int RegisterSPUpdatePos = 41;
    const int RegisterIUpdatePos = 44;
    const int RegisterRUpdatePos = 48;
    const int RegisterAUpdatePos = 52;
    const int RegisterIMUpdatePos = 54;
    const int RegisterINTENUpdatePos = 55;
    const int RegisterPCUpdatePos = 57;

    // Fill in the register values
    store16BitVal(regSetInstructions, TempSPUpdatePos, codeLocation+6);
    store16BitVal(regSetInstructions, RegisterIXUpdatePos, regs.IX);
    store16BitVal(regSetInstructions, RegisterIYUpdatePos, regs.IY);
    store16BitVal(regSetInstructions, RegisterHLDASHUpdatePos, regs.HLDASH);
    store16BitVal(regSetInstructions, RegisterDEDASHUpdatePos, regs.DEDASH);
    store16BitVal(regSetInstructions, RegisterBCDASHUpdatePos, regs.BCDASH);
    store16BitVal(regSetInstructions, RegisterSPUpdatePos, regs.SP);
    store16BitVal(regSetInstructions, RegisterHLUpdatePos, regs.HL);
    store16BitVal(regSetInstructions, RegisterDEUpdatePos, regs.DE);
    store16BitVal(regSetInstructions, RegisterBCUpdatePos, regs.BC);
    store16BitVal(regSetInstructions, RegisterAFDASHUpdatePos, regs.AFDASH);
    store16BitVal(regSetInstructions, RegisterAFUpdatePos, regs.AF);
    regSetInstructions[RegisterIUpdatePos] = regs.I;
    regSetInstructions[RegisterRUpdatePos] = (regs.R + 256 - 5) % 256;
    regSetInstructions[RegisterAUpdatePos] = regs.AF >> 8;
    regSetInstructions[RegisterIMUpdatePos] = (regs.INTMODE == 0) ? 0x46 : ((regs.INTMODE == 1) ? 0x56 : 0x5e);
    regSetInstructions[RegisterINTENUpdatePos] = (regs.INTENABLED == 0) ? 0xf3 : 0xfb;
    store16BitVal(regSetInstructions, RegisterPCUpdatePos, regs.PC);

    if (codeMaxlen >= sizeof(regSetInstructions))
    {
        memcopyfast(pCodeBuffer, regSetInstructions, sizeof(regSetInstructions));
        return sizeof(regSetInstructions);
    }
    return 0;
}
