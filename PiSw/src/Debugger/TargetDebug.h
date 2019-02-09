// Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../Machines/McBase.h"
#include "../System/ee_printf.h"
#include "../TargetBus/BusAccess.h"

class Z80Registers
{
public:
    uint16_t PC;
    uint16_t SP;
    uint16_t HL;
    uint16_t DE;
    uint16_t BC;
    uint16_t AF;
    uint16_t IX;
    uint16_t IY;
    uint16_t HLDASH;
    uint16_t DEDASH;
    uint16_t BCDASH;
    uint16_t AFDASH;
    uint16_t MEMPTR;
    uint8_t I;
    uint8_t R;
    uint8_t IMODE;
    uint8_t IFF;
    uint8_t VPS;

public:
    Z80Registers()
    {
        PC = SP = HL = DE = BC = AF = IX = IY = 0;
        HLDASH = DEDASH = BCDASH = AFDASH = MEMPTR = 0;
        I = R = IMODE = IFF = VPS = 0;
    }
    void format1(char* pResponse, int maxLen)
    {
        uint32_t curAddr = 0;
        uint32_t curData = 0;
        uint32_t curFlags = 0;
        BusAccess::pauseGetCurrent(&curAddr, &curData, &curFlags);
        char tmpStr[100];
        ee_sprintf(tmpStr, "PC=%04x SP=%04x BC=%04x AF=%04x HL=%04x DE=%04x IX=%04x IY=%04x",
                curAddr, SP, BC, AF, HL, DE, IX, IY);
        strlcpy(pResponse, tmpStr, maxLen);
        ee_sprintf(tmpStr, " AF'=%04x BC'=%04x HL'=%04x DE'=%04x I=%02x R=%02x  F=-------- F'=-------- MEMPTR=%04x IM%d IFF%02x VPS: %d",
                AFDASH, BCDASH, HLDASH, DEDASH, I, R, MEMPTR, IMODE, IFF, VPS );
        strlcat(pResponse, tmpStr, maxLen);
    }
};

class TargetDebug
{
public:
    TargetDebug()
    {
    }

    static TargetDebug* get();

    bool debuggerCommand(McBase* pMachine, [[maybe_unused]] const char* pCommand, 
            [[maybe_unused]] char* pResponse, [[maybe_unused]] int maxResponseLen);

    uint32_t handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal);

private:

    bool matches(const char* s1, const char* s2, int maxLen);
    void grabMemoryAndReleaseBusRq();

    Z80Registers _z80Registers;

    static const int MAX_MEM_DUMP_LEN = 1024;
    static const int MAX_TARGET_MEM_ADDR = 0xffff;
    static uint8_t _targetMemBuffer[MAX_MEM_DUMP_LEN];
};

extern TargetDebug __targetDebug;
