// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../System/RingBufferPosn.h"
#include "../TargetBus/TargetRegisters.h"
#include "../TargetBus/MemorySystem.h"
#include "libz80/z80.h"

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

class StepValidatorCycle
{
public:
    uint32_t addr;
    uint32_t data;
    uint32_t flags;
};

class StepValidator
{
public:
    StepValidator();

    void start();
    void stop();

    void service();

    void writeTestCode();

    uint32_t handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal);

private:

    // Z80 CPU context
    Z80Context _cpu_z80;

    // Memory system
    MemorySystem _emulatedMemory;

    // Singleton instance
    static StepValidator* _pThisInstance;

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
    int _excpCount;
    int _serviceCount;
    bool _isActive;

    uint32_t volatile _debugCurChVal;
    uint32_t volatile _debugStepCount;

    // Test cases
    void testCase1();
};
