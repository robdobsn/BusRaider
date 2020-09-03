// Bus Raider Hardware RC2014 RAMROM
// Rob Dobson 2019

#include "HwBase.h"

class HwRAMROM : public HwBase
{
public:
    HwRAMROM(HwManager& hwManager, BusAccess& busAccess);

    // Configure
    virtual void configure(const char* jsonConfig);

    // Page out RAM/ROM due to emulation
    virtual void setMemoryEmulationMode(bool pageOut);

    // Set paging enable
    virtual void setMemoryPagingEnable(bool val)
    {
        _pageOutEnabled = val;
    }

    // Mirror mode
    virtual void setMirrorMode(bool val);
    virtual void mirrorClone();
    
    // Page out RAM/ROM for opcode injection
    virtual void pageOutForInjection(bool pageOut);

    // Block access to hardware
    virtual BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len, 
                bool busRqAndRelease, bool iorq, bool forceMirrorAccess);
    virtual BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, 
                bool busRqAndRelease, bool iorq, bool forceMirrorAccess);

    // Get mirror memory for address
    uint8_t* getMirrorMemForAddr(uint32_t addr);

    // Tracer interface to hardware
    virtual void tracerClone();
    virtual void tracerHandleAccess(uint32_t addr, uint32_t data, 
            uint32_t flags, uint32_t& retVal);

    // Handle a completed bus action
    virtual void handleBusActionComplete(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

    // Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
    virtual void handleMemOrIOReq(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& retVal);

    // Max address
    virtual uint32_t getMaxAddress()
    {
        return (_memCardSizeBytes * 1024) - 1;
    }

private:
    // Paging hardware support
    bool _memoryEmulationMode;
    bool _pageOutEnabled;
    bool _currentlyPagedOut;

    // Mirror mode
    bool _mirrorMode;

    // Mirrored memory (used for debugging and memory-emulation modes)
    uint8_t* _pMirrorMemory;
    uint32_t _mirrorMemoryLen;
    bool _mirrorMemAllocNotified;
    uint8_t* getMirrorMemory();

    // Tracer memory (used for emulated CPU step-validation)
    static const uint32_t TRACER_DEFAULT_MEM_SIZE_K = 64;
    uint8_t* _pTracerMemory;
    uint32_t _tracerMemoryLen;
    bool _tracerMemAllocNotified;
    uint8_t* getTracerMemory();

    // Size of memory card
    static const uint32_t DEFAULT_MEM_SIZE_K = 1024;
    uint32_t _memCardSizeBytes;

    // Operation mode of memory card
    enum memoryCardOpMode_t {
        MEM_CARD_OP_MODE_LINEAR,
        MEM_CARD_OP_MODE_BANKED
    };
    memoryCardOpMode_t _memoryCardOpMode;

    // Rob's memory card options
    enum memOpts_t {
        MEM_OPT_OPT_NONE = 0x00,
        MEM_OPT_STAY_BANKED = 0x01,
        MEM_OPT_EMULATE_LINEAR = 0x04,
        MEM_OPT_EMULATE_LINEAR_UPPER = 0x08
    };
    memOpts_t _memCardOpts;

    // Mem card opts
    const char* getMemCardOptStr(memOpts_t memOpt)
    {
        switch (memOpt)
        {
        case MEM_OPT_STAY_BANKED: return "StayBanked";
        case MEM_OPT_EMULATE_LINEAR: return "EmulateLinear";
        case MEM_OPT_EMULATE_LINEAR_UPPER: return "EmulateLinearUpper";
        default: return "UnknownOption";
        }
    }
    
    // Memory card using 74670 register files to bank 16K pages
    static const int NUM_BANKS = 4;
    uint8_t _bankRegisters[NUM_BANKS];
    bool _bankRegisterOutputEnable;
    uint32_t _bankHwBaseIOAddr;
    uint32_t _bankHwPageEnIOAddr;

    // Scott Baker / Spencer Owen / Sergey Kiselevs RC2014 RAM/ROM card addresses
    static const int BANK_16K_BASE_ADDR = 0x78;
    static const int BANK_16K_PAGE_ENABLE = 0x7c;
    static const int BANK_16K_LIN_TO_PAGE = 0x7e;
    static const int BANK_SIZE_BYTES = 16384;
    
    // Reset
    void hwReset();

    // Access linear or banked memory
    BR_RETURN_TYPE physicalBlockAccess(uint32_t addr, const uint8_t* pBuf, uint32_t len,
            bool busRqAndRelease, bool iorq, bool write);
    void setBanksToEmulate64KAddrSpace(bool upperChip);
    BR_RETURN_TYPE readWriteBankedMemory(uint32_t addr, uint8_t* pBuf, uint32_t len,
            bool iorq, bool write);

    // Base name
    static const char* _baseName;
};
