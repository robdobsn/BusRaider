// Bus Raider Hardware Base Class
// Rob Dobson 2019

#pragma once
#include <stdint.h>

class HwBase
{
  public:

    HwBase();

    // Reset
    virtual void reset();

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual uint32_t handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags, uint32_t retVal);

    // Page out RAM/ROM due to emulation
    virtual void pageOutForEmulation(bool pageOut);

    // Page out RAM/ROM for opcode injection
    virtual void pageOutForInjection(bool pageOut);

    // Check if paging requires bus access
    virtual bool pagingRequiresBusAccess();

};
