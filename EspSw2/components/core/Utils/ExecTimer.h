/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExecTimer
// Timer for an execution path
//
// Rob Dobson 2018-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Utils.h>
#include "ArduinoTime.h"

class ExecTimer
{
public:
    ExecTimer()
    {
        clear();
    };

    void clear()
    {
        _execStartTimeUs = 0;
        _execMaxTimeUs = 0;
    }

    // Record that execution has started
    inline void started()
    {
        _execStartTimeUs = micros();
    }

    // Record that execution has ended
    inline void ended()
    {
        unsigned long durUs = Utils::timeElapsed(micros(), _execStartTimeUs);
        if (_execMaxTimeUs < durUs)
            _execMaxTimeUs = durUs;
    }

    // Valid?
    bool valid() const
    {
        return _execMaxTimeUs != 0;
    }

    // Get max
    uint64_t getMaxUs() const
    {
        return _execMaxTimeUs;
    }

    // Vars
    uint64_t _execStartTimeUs;
    uint64_t _execMaxTimeUs;
};
