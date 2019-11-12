// Bus Raider Hardware RC2014 512K RAM/ROM
// Rob Dobson 2019

#include "HwBase.h"

class Hw1MBRamRom : public HwBase
{

public:

    Hw1MBRamRom();

    // Page out RAM/ROM due to emulation
    virtual void setMemoryEmulationMode(bool pageOut);

    // Set paging enable
    virtual void setMemoryPagingEnable(bool val)
    {
        _pagingEnabled = val;
    }
    
    // Page out RAM/ROM for opcode injection
    virtual void pageOutForInjection(bool pageOut);

    // Handle a completed bus action
    virtual void handleBusActionComplete(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

private:
    static const char* _logPrefix;
    bool _memoryEmulationMode;
    bool _pagingEnabled;
    static const int NUM_BANKS = 4;
    uint8_t _bankRegisters[NUM_BANKS];
    static uint8_t _pageOutAllBanks[NUM_BANKS];
    
    // Reset
    void hwReset();

    // Base name
    static const char* _baseName;
    
};
