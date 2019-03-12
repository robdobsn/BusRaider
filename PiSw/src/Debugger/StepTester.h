// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../System/RingBufferPosn.h"
#include "../TargetBus/TargetRegisters.h"

class StepTesterException
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

class StepTester
{
public:
    StepTester();

    void service();

    void writeTestCode();

    uint32_t handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal);

private:

    // Registers
    Z80Registers _regs;

    // Exception list
    static const int NUM_DEBUG_VALS = 200;
    volatile StepTesterException _exceptions[NUM_DEBUG_VALS];
    RingBufferPosn _exceptionsPosn;
    int _excpCount;
    int _serviceCount;

    int32_t volatile _debugCurBCCount;
    uint32_t volatile _debugCurChVal;
    uint32_t volatile _debugCurAddr;
    uint32_t volatile _debugStepCount;

};
