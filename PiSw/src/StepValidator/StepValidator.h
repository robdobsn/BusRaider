// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../System/RingBufferPosn.h"
#include "../TargetBus/TargetRegisters.h"
#include "../TargetBus/BusAccess.h"
#include "../CommandInterface/CommandHandler.h"
#include "libz80/z80.h"

// #define STEP_VAL_WITHOUT_HW_MANAGER 1
#ifndef STEP_VAL_WITHOUT_HW_MANAGER
#include "../Hardware/HwManager.h"
#endif

// Stats
class StepValidatorStats
{
public:
    StepValidatorStats()
    {
        clear();
    }
    void clear()
    {
        isrCalls = 0;
        errors = 0;
        instructionCount = 0;
    }
    uint32_t isrCalls;
    uint32_t errors;
    uint32_t instructionCount;
};

// Exceptions
class StepValidatorException
{
public:
    uint32_t addr;
    uint32_t expectedAddr;
    uint32_t dataFromZ80;
    uint32_t expectedData;
    uint32_t dataToZ80;
    uint32_t flags;
    uint32_t expectedFlags;
    uint32_t stepCount;
};

// Cycle info
class StepValidatorCycle
{
public:
    uint32_t addr;
    uint32_t data;
    uint32_t flags;
};

// Validator
class StepValidator
{
public:
    StepValidator();
    void init();

    // Control
    void start();
    void stop();
    void primeFromMem();
    
    // Service
    void service();

    // Stats
    StepValidatorStats& getStats();
    
private:

    // Z80 CPU context
    Z80Context _cpu_z80;

    // Memory system
#ifdef STEP_VAL_WITHOUT_HW_MANAGER
    uint8_t* _pValidatorMemory;
    uint32_t _validatorMemoryLen;
#endif

    // Singleton instance
    static StepValidator* _pThisInstance;

    // Bus socket we're attached to
    static int _busSocketId;
    static BusSocketInfo _busSocketInfo;

    // Comms socket we're attached to
    static int _commsSocketId;
    static CommsSocketInfo _commsSocketInfo;

    // Handle messages (telling us to start/stop)
    static bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);

    // Get status
    void getStatus(char* pRespJson, int maxRespLen, const char* statusIdxStr);

    // Reset complete callback
    static void busActionCompleteStatic(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);
    void resetComplete();

    // Wait interrupt handler
    static void handleWaitInterruptStatic(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);
    void handleWaitInterrupt(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Memory and IO functions
    static byte mem_read(int param, ushort address);
    static void mem_write(int param, ushort address, byte data);
    static byte io_read(int param, ushort address);
    static void io_write(int param, ushort address, byte data);

    // Record step information for instruction
    static const int MAX_STEP_CYCLES_FOR_INSTR = 10;
    static StepValidatorCycle _stepCycles[MAX_STEP_CYCLES_FOR_INSTR];
    static int _stepCycleCount;
    static int _stepCyclePos;

    // Exception list
    static const int NUM_DEBUG_VALS = 200;
    volatile StepValidatorException _exceptions[NUM_DEBUG_VALS];
    RingBufferPosn _exceptionsPosn;
    bool _isActive;

    // Stats
    StepValidatorStats _stats;

    // Service count
    int _serviceCount;
};
