// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "BusAccess.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SimpleBreakpoint
{
public:
    static const int MAX_HIT_MSG_LEN = 100;
    bool enabled;
    char hitMessage[MAX_HIT_MSG_LEN];
    uint32_t pcValue;

    SimpleBreakpoint()
    {
        enabled = false;
        hitMessage[0] = 0;
        pcValue = 0;
    }
};

class TargetBreakpoints
{
public:

    // Construct
    TargetBreakpoints();

    // Control
    void enableBreakpoints(bool en)
    {
        _breakpointsEnabled = en;
    }
    void enableBreakpoint(int idx, bool enabled);
    void setBreakpointMessage(int idx, const char* hitMessage);
    void setBreakpointPCAddr(int idx, uint32_t pcVal);
    bool checkForBreak(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);
    int getNumEnabled()
    {
        return _breakpointNumEnabled;
    }
    int isEnabled()
    {
        return _breakpointsEnabled;
    }
    void setFastBreakpoint(uint32_t addr, bool en);
    void clearFastBreakpoints()
    {
        _fastBreakpointsNumEnabled = 0;
    }

private:
    void clear();
    bool _breakpointsEnabled;
    int _breakpointNumEnabled;
    static const int MAX_BREAKPOINTS = 100;
    SimpleBreakpoint _breakpoints[MAX_BREAKPOINTS];
    int _breakpointIdxsToCheck[MAX_BREAKPOINTS];
    int _breakpointHitIndex;
    int _fastBreakpointsNumEnabled;
    SimpleBreakpoint _fastBreakpoints[MAX_BREAKPOINTS];
    int _fastBreakpointHitIdx;
};
