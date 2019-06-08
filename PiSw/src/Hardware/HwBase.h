// Bus Raider Hardware Base Class
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include "../TargetBus/TargetCPU.h"

class HwBase
{
public:

    HwBase();

    // Handle a completed bus action
    virtual void handleBusActionComplete(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

    // Set emulation mode
    virtual void setMemoryEmulationMode(bool val);

    // Set paging enable
    virtual void setMemoryPagingEnable(bool val);

    // Page out RAM/ROM for opcode injection
    virtual void pageOutForInjection(bool pageOut);

    // Mirror mode
    virtual void setMirrorMode(bool val);
    virtual void mirrorClone();

    // Block access to hardware
    virtual BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);
    virtual BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);

    // Get mirror memory for address
    virtual uint8_t* getMirrorMemForAddr(uint32_t addr);

    // Tracer interface to hardware
    virtual void tracerClone();
    virtual void tracerHandleAccess(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Is enabled
    virtual bool isEnabled()
    {
        return _enabled;
    }

    // Enable
    virtual void enable(bool en)
    {
        _enabled = en;
    }

    // Name
    virtual const char* name()
    {
        return _pName;
    }

    // Configure
    virtual void configure(const char* name, const char* jsonConfig);

protected:
    bool _enabled;
    const char* _pName;
};
