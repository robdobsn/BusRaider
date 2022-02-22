// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetRegisters.h"

#define Z80_PROGRAM_RESET_VECTOR 0

class TargetCPUZ80
{
public:

    static uint32_t getInjectToSetRegs(Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen);
    static uint32_t getSnippetToSetRegs(uint32_t codeLocation, Z80Registers& regs, uint8_t* pCodeBuffer, uint32_t codeMaxlen);
    static void store16BitVal(uint8_t arry[], int offset, uint16_t val);

};
