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

typedef void SendDebugMessageType(const char* pStr);

class Breakpoint
{
public:
    static const int MAX_HIT_MSG_LEN = 100;
    bool enabled;
    char hitMessage[MAX_HIT_MSG_LEN];
    uint32_t pcValue;

    Breakpoint()
    {
        enabled = false;
        hitMessage[0] = 0;
        pcValue = 0;
    }
};

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
    // Max size of emulated target memory
    static const uint32_t MAX_TARGET_MEMORY_LEN = 0x10000;

public:
    TargetDebug();

    static TargetDebug* get();
    void service();

    bool debuggerCommand(McBase* pMachine, [[maybe_unused]] const char* pCommand, 
            [[maybe_unused]] char* pResponse, [[maybe_unused]] int maxResponseLen);

    uint32_t handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal,
            [[maybe_unused]] McDescriptorTable& descriptorTable);

    // Comms callback
    void setSendDebugMessageCallback(SendDebugMessageType* pSendDebugMessageCallback)
    {
        _pSendDebugMessageCallback = pSendDebugMessageCallback;
    }

    uint8_t getMemoryByte(uint32_t addr);
    uint16_t getMemoryWord(uint32_t addr);
    void clearMemory();
    bool blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len);
    bool blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len);

private:

    bool matches(const char* s1, const char* s2, int maxLen);
    void grabMemoryAndReleaseBusRq(McBase* pMachine, bool singleStep);
    void enableBreakpoint(int idx, bool enabled);
    void setBreakpointMessage(int idx, const char* hitMessage);
    void setBreakpointPCAddr(int idx, uint32_t pcVal);
    void startGetRegisterSequence();

    // Callback to send debug frames
    static SendDebugMessageType* _pSendDebugMessageCallback;

    // Registers
    Z80Registers _z80Registers;

    // Register query mode
    bool _registerQueryMode;
    bool _registerQueryGotM1;
    int _registerQueryWriteIndex;
    unsigned int _registerQueryStep;

    // Target memory buffer  
    static const int MAX_TARGET_MEM_ADDR = 0xffff;
    static uint8_t _targetMemBuffer[MAX_TARGET_MEMORY_LEN];

    // Limit on data sent back
    static const int MAX_MEM_DUMP_LEN = 1024;

    // Breakpoints
    static const int MAX_BREAKPOINTS = 100;
    static bool _breakpointsEnabled;
    static int _breakpointNumEnabled;
    static int _breakpointIdxsToCheck[MAX_BREAKPOINTS];
    static Breakpoint _breakpoints[MAX_BREAKPOINTS];
    static bool _breakpointHitFlag;
    static int _breakpointHitIndex;
};

extern TargetDebug __targetDebug;
