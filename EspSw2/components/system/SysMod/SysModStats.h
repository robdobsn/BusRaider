// RBotFirmware
// Rob Dobson 2019

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Stats maintained on SysMods
// Each system module's execution is timed
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once 

#include <stdint.h>
#include <Utils.h>

class SysModStats
{
public:
    SysModStats()
    {
        clear();
    };

    void clear()
    {
        _execStartTimeUs = 0;
        _execMaxTimeUs = 0;
    }

    // Record that module execution has started
    inline void started()
    {
        _execStartTimeUs = micros();
    }

    // Record that module execution has ended
    inline void ended()
    {
        unsigned long durUs = Utils::timeElapsed(micros(), _execStartTimeUs);
        if (_execMaxTimeUs < durUs)
            _execMaxTimeUs = durUs;
    }

    // Vars
    uint64_t _execStartTimeUs;
    uint64_t _execMaxTimeUs;
};
