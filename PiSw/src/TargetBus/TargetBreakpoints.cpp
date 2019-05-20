// Bus Raider
// Rob Dobson 2019

#include "TargetBreakpoints.h"
#include "../System/lowlib.h"
#include "../System/ee_sprintf.h"
#include "../System/logging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromTargetBreakpoints[] = "TargetBreakpoints";

TargetBreakpoints::TargetBreakpoints()
{
    clear();
}

void TargetBreakpoints::clear()
{
    for (int i = 0; i < MAX_BREAKPOINTS; i++)
    {
        _breakpoints[i].enabled = false;
        _fastBreakpoints[i].enabled = false;
        ee_sprintf(_fastBreakpoints[i].hitMessage, "FB%02d", i);
    }
    _breakpointNumEnabled = 0;
    _breakpointsEnabled = true;
    _breakpointHitIndex = 0;
    _fastBreakpointsNumEnabled = 0;
    _fastBreakpointHitIdx = 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Breakpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetBreakpoints::enableBreakpoint(int idx, bool enabled)
{
    if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
        return;
    _breakpoints[idx].enabled = enabled;
    _breakpointNumEnabled = 0;
    int numBreakpointsEnabled = 0;
    for (int i = 0; i < MAX_BREAKPOINTS; i++)
        if (_breakpoints[i].enabled)
            _breakpointIdxsToCheck[numBreakpointsEnabled++] = i;
    _breakpointNumEnabled = numBreakpointsEnabled;
}

void TargetBreakpoints::setBreakpointMessage(int idx, const char* hitMessage)
{
    if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
        return;
    if (hitMessage != NULL)
        strlcpy(_breakpoints[idx].hitMessage, hitMessage, SimpleBreakpoint::MAX_HIT_MSG_LEN);
}

void TargetBreakpoints::setBreakpointPCAddr(int idx, uint32_t pcVal)
{
    if ((idx < 0) || (idx >= MAX_BREAKPOINTS))
        return;
    _breakpoints[idx].pcValue = pcVal;
}

bool TargetBreakpoints::checkForBreak([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
    LogWrite(FromTargetBreakpoints, LOG_DEBUG, "checkForBreak %04x, fast %d", 
            addr, _fastBreakpointsNumEnabled);
    // See if fast-breakpoints enabled and M1 cycle
    if ((_fastBreakpointsNumEnabled > 0) && (flags & BR_CTRL_BUS_M1_MASK) && (flags & BR_CTRL_BUS_RD_MASK))
    {
        // Check breakpoints
        for (int i = 0; i < _fastBreakpointsNumEnabled; i++)
        {
            if (_fastBreakpoints[i].pcValue == addr)
            {
                _fastBreakpointHitIdx = i;
                return true;
            }
        }
    }
    // See if breakpoints enabled and M1 cycle
    if (_breakpointsEnabled && (_breakpointNumEnabled > 0) && (flags & BR_CTRL_BUS_M1_MASK) && (flags & BR_CTRL_BUS_RD_MASK))
    {
        // Check breakpoints
        for (int i = 0; i < _breakpointNumEnabled; i++)
        {
            int bpIdx = _breakpointIdxsToCheck[i];
            if (_breakpoints[bpIdx].pcValue == addr)
            {
                _breakpointHitIndex = bpIdx;
                return true;
            }
        }
    }
    return false;
}

void TargetBreakpoints::setFastBreakpoint(uint32_t addr, bool en)
{
    // Check for change to existing
    for (int i = 0; i < _fastBreakpointsNumEnabled; i++)
    {
        if (_fastBreakpoints[i].pcValue == addr)
        {
            _fastBreakpoints[i].enabled = en;
            return;
        }
    }
    if (en && (_fastBreakpointsNumEnabled < MAX_BREAKPOINTS))
    {
        _fastBreakpoints[_fastBreakpointsNumEnabled].enabled = true;
        _fastBreakpoints[_fastBreakpointsNumEnabled].pcValue = addr;
        _fastBreakpointsNumEnabled++;
    }
}
