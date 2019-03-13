// Bus Raider Hardware RC2014 64K RAM
// Rob Dobson 2019

#include "HwBase.h"

class Hw64KPagedRam : public HwBase
{
private:
    static const char* _logPrefix;
    bool _pageOutForEmulation;
    bool _pagingEnabled;
    bool _currentlyPagedOut;

public:

    Hw64KPagedRam();

    // Reset
    virtual void reset();
    
    // Page out RAM/ROM due to emulation
    virtual void pageOutForEmulation(bool restore);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual uint32_t handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags, uint32_t retVal);

    // Page out RAM/ROM for opcode injection
    virtual void pageOutForInjection(bool restore);

    // Check if paging requires bus access
    virtual bool pagingRequiresBusAccess();
};
