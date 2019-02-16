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
#include "../TargetBus/TargetRegisters.h"

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

    // Memory access
    uint8_t getMemoryByte(uint32_t addr);
    uint16_t getMemoryWord(uint32_t addr);
    void clearMemory();
    bool blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len);
    bool blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len);

    // Register injection
    void startSetRegisterSequence(Z80Registers* pRegs = NULL);

private:

    bool matches(const char* s1, const char* s2, int maxLen);
    void grabMemoryAndReleaseBusRq(McBase* pMachine, bool singleStep);
    void enableBreakpoint(int idx, bool enabled);
    void setBreakpointMessage(int idx, const char* hitMessage);
    void setBreakpointPCAddr(int idx, uint32_t pcVal);
    void startGetRegisterSequence();
    void handleRegisterGet(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    void handleRegisterSet(uint32_t& retVal);

    // Callback to send debug frames
    static SendDebugMessageType* _pSendDebugMessageCallback;

    // Registers
    Z80Registers _z80Registers;

    // Register mode
    enum REGISTER_MODE {
        REGISTER_MODE_NONE,
        REGISTER_MODE_GET,
        REGISTER_MODE_SET,
    };
    REGISTER_MODE _registerMode;
    bool _registerModeGotM1;
    int _registerQueryWriteIndex;
    unsigned int _registerModeStep;

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
