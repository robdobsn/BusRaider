// Bus Raider Hardware RC2014 512K RAM/ROM
// Rob Dobson 2019

#include "HwBase.h"

class Hw512KRamRom : public HwBase
{
private:
    static const char* _logPrefix;
    bool _pageOutForEmulation;
    bool _pagingEnabled;
    static const int NUM_BANKS = 4;
    uint8_t _bankRegisters[NUM_BANKS];
    static uint8_t _pageOutAllBanks[NUM_BANKS];

public:

    Hw512KRamRom();

    // Reset
    virtual void reset();
    
    // Page out RAM/ROM due to emulation
    virtual void pageOutForEmulation(bool pageOut);

    // Page out RAM/ROM for opcode injection
    virtual void pageOutForInjection(bool pageOut);

    // Check if paging requires bus access
    virtual bool pagingRequiresBusAccess();

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual uint32_t handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags, uint32_t retVal);

};
