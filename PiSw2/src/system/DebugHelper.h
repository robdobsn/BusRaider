// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "PiWiring.h"

// Pi debug pin
#define BR_DEBUG_PI_SPI0_CE0 8 // SPI0 CE0
#define BR_DEBUG_PI_SPI0_CE0_MASK (1 << BR_DEBUG_PI_SPI0_CE0) // SPI0 CE0

class DebugHelper
{
public:
    static const uint32_t NUM_DEBUG_VALS = 15;
    DebugHelper()
    {
        for (uint32_t i = 0; i < NUM_DEBUG_VALS; i++)
        {
            _vals[i] = 0;
            _valIsValid[i] = false;
        }
    }
    void set(uint32_t valIdx, int val)
    {
        if (valIdx < NUM_DEBUG_VALS)
        {
            _valIsValid[valIdx] = true;
            _vals[valIdx] = val;
        }
    }
    void peak(uint32_t valIdx, int val)
    {
        if (valIdx < NUM_DEBUG_VALS)
        {
            if (!_valIsValid[valIdx])
                _vals[valIdx] = INT32_MIN;
            _valIsValid[valIdx] = true;
            if (_vals[valIdx] < val)
                _vals[valIdx] = val;
        }
    }
    void inc(uint32_t valIdx)
    {
        if (valIdx < NUM_DEBUG_VALS)
        {
            if (!_valIsValid[valIdx])
                _vals[valIdx] = 0;
            _valIsValid[valIdx] = true;
            _vals[valIdx]++;
        }
    }
    bool get(uint32_t valIdx, int& val)
    {
        if ((valIdx >= NUM_DEBUG_VALS) || !_valIsValid[valIdx])
            return false;
        val = _vals[valIdx];
        return true;
    }
    void pulse()
    {
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    }
    void nopulse()
    {
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
    }
    int _vals[NUM_DEBUG_VALS];
    bool _valIsValid[NUM_DEBUG_VALS];
};

extern DebugHelper __debugHelper;

#define DEBUG_VAL_SET(valIdx,val) __debugHelper.set(valIdx, val)
#define DEBUG_VAL_PEAK(valIdx,val) __debugHelper.peak(valIdx, val)
#define DEBUG_VAL_INC(valIdx) __debugHelper.inc(valIdx)
#define DEBUG_PULSE() __debugHelper.pulse();
#define DEBUG_NO_PULSE() __debugHelper.nopulse();
