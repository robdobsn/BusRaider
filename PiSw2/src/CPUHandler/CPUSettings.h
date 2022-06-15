// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "BusAccessDefs.h"

class CPUSettings
{
public:
    CPUSettings()
    {
        resetDurationMs = BR_RESET_PULSE_MS;
        memSpeedNs = BR_MEM_SPEED_DEFAULT_NS;
    }
    uint32_t resetDurationMs;
    uint32_t memSpeedNs;
};
