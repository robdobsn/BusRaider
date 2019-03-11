// Bus Raider Hardware Base Class
// Rob Dobson 2019

#pragma once
#include <stdint.h>

class HwBase
{
  public:

    HwBase();

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual uint32_t handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags);

    // Page out RAM/ROM for opcode injection
    virtual void pageOutRamRom(bool restore);

    // Check if paging requires bus access
    virtual bool pagingRequiresBusAccess();

};
