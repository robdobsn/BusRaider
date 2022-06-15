// Bus Raider
// Rob Dobson 2019-2022

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetImager.h"
#include "CPUProgrammerIF.h"

class MemoryInterface;
class HwManager;
class CPUHandler;

class TargetProgrammer : public CPUProgrammerIF
{
public:
    TargetProgrammer(MemoryInterface& memoryInterface,
        CPUHandler& cpuHandler, HwManager& hwManager,
        TargetImager& _targetImager
    );

    void clear();
    void programmingStart(bool execAfterProgramming, bool enterDebugger);

private:
    // Elements
    MemoryInterface& _memoryInterface;
    CPUHandler& _cpuHandler;
    HwManager& _hwManager;

    // Target Imager
    TargetImager& _targetImager;

    // Programming state
    bool _programmingStartPending;
    bool _programmingDoExec;
    bool _programmingEnterDebugger;

    void programmingWrite();
    void programExec(bool codeAtResetVector);

    virtual bool isProgrammingPending()
    {
        return _programmingStartPending;
    }

    virtual void programmingHasStarted() override final
    {
        _programmingStartPending = false;
    }
};
