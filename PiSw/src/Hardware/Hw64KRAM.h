// Bus Raider Hardware RC2014 64K RAM
// Rob Dobson 2019

#include "HwBase.h"

class Hw64KRam : public HwBase
{
public:
    Hw64KRam();

    // Page out RAM/ROM due to emulation
    virtual void setMemoryEmulationMode(bool pageOut);

    // Set paging enable
    virtual void setMemoryPagingEnable(bool val)
    {
        _pagingEnabled = val;
    }

    // Page out RAM/ROM for opcode injection
    virtual void pageOutForInjection(bool pageOut);

    // Block access to hardware
    virtual BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);
    virtual BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, bool busRqAndRelease, bool iorq);

    // Get mirror memory for address
    uint8_t* getMirrorMemForAddr(uint32_t addr);

    // Validator interface to hardware
    virtual void validatorClone();
    virtual void validatorHandleAccess(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Handle a completed bus action
    virtual void handleBusActionComplete(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

private:
    static const char* _logPrefix;

    // Paging hardware support
    bool _memoryEmulationMode;
    bool _pagingEnabled;
    bool _currentlyPagedOut;

    // Mirrored memory (used for debugging and memory-emulation modes)
    uint8_t* _pMirrorMemory;
    uint32_t _mirrorMemoryLen;
    uint8_t* getMirrorMemory();

    // Validator memory (used for emulated CPU step-validation)
    uint8_t* _pValidatorMemory;
    uint32_t _validatorMemoryLen;
    uint8_t* getValidatorMemory();

    // Reset
    void hwReset();

    // Base name
    static const char* _baseName;
};
