// Bus Raider Hardware RC2014 RAMROM
// Rob Dobson 2019

#include "HwRAMROM.h"
#include "HwManager.h"
#include "BusAccess.h"
#include "rdutils.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>

static const char* MODULE_PREFIX = "HWRAMROM";
const char* HwRAMROM::_baseName = "RAMROM";

HwRAMROM::HwRAMROM(HwManager& hwManager, BusAccess& busAccess) : 
        HwBase(hwManager, busAccess)
{
    _memCardSizeBytes = DEFAULT_MEM_SIZE_K*1024;
    _mirrorMemoryLen = _memCardSizeBytes;
    _pMirrorMemory = NULL;
    _mirrorMemAllocNotified = false;
    _tracerMemoryLen = TRACER_DEFAULT_MEM_SIZE_K*1024;
    _pTracerMemory = NULL;
    _tracerMemAllocNotified = false;
    _pName = _baseName;
    _isEmulatingMemory = false;
    _memoryCardOpMode = MEM_CARD_OP_MODE_LINEAR;
    _memCardOpts = MEM_OPT_OPT_NONE;
    _bankHwBaseIOAddr = BANK_16K_BASE_ADDR;
    _bankHwPageEnIOAddr = BANK_16K_PAGE_ENABLE;
    _pageOutEnabled = true;
    _bankRegisterOutputEnable = false;
    for (int i = 0; i < NUM_BANKS; i++)
        _bankRegisters[i] = 0;
    _currentlyPagedOut = false;
    hwReset();
}

// Configure
void HwRAMROM::configure( const char* jsonConfig)
{
    // Get values from JSON
    static const int MAX_CMD_PARAM_STR = 100;

    // Get memSize param
    char memSizeStr[MAX_CMD_PARAM_STR+1];
    uint32_t memSizeK = DEFAULT_MEM_SIZE_K;
    if (jsonGetValueForKey("memSizeK", jsonConfig, memSizeStr, MAX_CMD_PARAM_STR))
        memSizeK = strtoul(memSizeStr, NULL, 10);

    // Get linear/banked param
    char paramStr[MAX_CMD_PARAM_STR+1];
    _memoryCardOpMode = MEM_CARD_OP_MODE_LINEAR;
    if (jsonGetValueForKey("bankHw", jsonConfig, paramStr, MAX_CMD_PARAM_STR))
    {
        if ((strcasecmp(paramStr, "BANKED") == 0) || (strcasecmp(paramStr, "PAGED") == 0))
            _memoryCardOpMode = MEM_CARD_OP_MODE_BANKED;
    }

    // Get rob's linear/banked card options
    _memCardOpts = MEM_OPT_OPT_NONE;
    if (jsonGetValueForKey("memOpt", jsonConfig, paramStr, MAX_CMD_PARAM_STR))
    {
        if (strcasecmp(paramStr, "STAYBANKED") == 0)
            _memCardOpts = MEM_OPT_STAY_BANKED;
        if (strcasecmp(paramStr, "EMULATELINEAR") == 0)
            _memCardOpts = MEM_OPT_EMULATE_LINEAR;
        else if (strcasecmp(paramStr, "EMULATELINEARUPPER") == 0)
            _memCardOpts = MEM_OPT_EMULATE_LINEAR_UPPER;
    }

    // Get pageOut param
    _pageOutEnabled = true;
    if (jsonGetValueForKey("pageOut", jsonConfig, paramStr, MAX_CMD_PARAM_STR))
    {
        if (strcasecmp(paramStr, "busPAGE") != 0)
            _pageOutEnabled = false;
    }

    uint32_t newMemSizeBytes = memSizeK*1024;

    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "curMemSizeBytes %d newMemSizeBytes %d memSizeK %d param %s", 
    //         _memCardSizeBytes, newMemSizeBytes, memSizeK, memSizeStr);

    if (_memCardSizeBytes != newMemSizeBytes)
    {
        _memCardSizeBytes = newMemSizeBytes;
        _mirrorMemoryLen = _memCardSizeBytes;
        if (_pMirrorMemory)
        {
            delete [] _pMirrorMemory;
            _pMirrorMemory = NULL;
        }
        // _tracerMemoryLen = _memCardSizeBytes;
        // if (_pTracerMemory)
        // {
        //     delete [] _pTracerMemory;
        //     _pTracerMemory = NULL;
        // }
    }

    LogWrite(MODULE_PREFIX, LOG_DEBUG, "configure Paging %s, Mode %s, Opts %x, MemSize %d (%dK) ... json %s", 
                _pageOutEnabled ? "Y" : "N",
                _memoryCardOpMode == MEM_CARD_OP_MODE_LINEAR ? "Linear" : "Banked",
                _memCardOpts,
                _memCardSizeBytes,
                memSizeK,
                jsonConfig);
}

// Set memory emulation mode - page out real RAM/ROM and use mirror memory
// for bus accesses
void HwRAMROM::setMemoryEmulationMode(bool pageOut)
{
    _isEmulatingMemory = pageOut;

    // Paging
    if (!_pageOutEnabled)
        return;

    // Debug
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "setMemoryEmulationMode %s", pageOut ? "Y" : "N");

    // Page out
    if (pageOut)
    {
        // Page out
        _busAccess.busPagePinSetActive(true);
    }
    else
    {
        // Release paging unless otherwise paged out
        if (!_currentlyPagedOut)
            _busAccess.busPagePinSetActive(false);
    }
}

// Page out RAM/ROM for opcode injection
void HwRAMROM::pageOutForInjection(bool pageOut)
{
    _currentlyPagedOut = pageOut;

    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "pageOutForInjection %s pagingEn %d", pageOut ? "Y" : "N", _pageOutEnabled);

    // Check enabled
    if (!_pageOutEnabled)
        return;
        
    // Page out
    if (pageOut)
    {
        _busAccess.busPagePinSetActive(true);
    }
    else
    {
        // Release paging unless emulation in progress
        if (!_isEmulatingMemory)
            _busAccess.busPagePinSetActive(false);
    }
}

// Hardware reset has occurred
void HwRAMROM::hwReset()
{
}

// Mirror mode
void HwRAMROM::setMirrorMode(bool val)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Mirror mode %d", val);
    _mirrorMode = val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mirror memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t* HwRAMROM::getMirrorMemory()
{
    if (!_pMirrorMemory)
    {
        _pMirrorMemory = new uint8_t[_mirrorMemoryLen];
        if (!_mirrorMemAllocNotified)
        {
            _mirrorMemAllocNotified = true;
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "Alloc for mirror memory len %d %s", 
                    _mirrorMemoryLen, _pMirrorMemory ? "OK" : "FAIL");
        }
    }
    return _pMirrorMemory;
}

void HwRAMROM::mirrorClone()
{
    // No point doing this in emulator mode as mirror and emulator memory is shared
    if (_isEmulatingMemory)
        return;

    // Dest memory
    uint8_t* pDestMemory = getMirrorMemory();
    if (!pDestMemory)
        return;

    // int blockReadResult = 
    _busAccess.blockRead(0, pDestMemory, _mirrorMemoryLen, false, false);
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "mirrorClone blockRead %s %02x %02x %02x",
    //             (blockReadResult == BR_OK) ? "OK" : "FAIL",
    //             pDestMemory[0], pDestMemory[1], pDestMemory[2]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Emulate 64K linear memory with banked memory card
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HwRAMROM::setBanksToEmulate64KAddrSpace(bool upperChip)
{
    // Write consecutive bank numbers to all bank registers 
    if (upperChip)
    {
        const uint8_t bankNumData[] = { 32, 33, 34, 35 };
        _busAccess.blockWrite(BANK_16K_BASE_ADDR, bankNumData, 4, false, true);
    }
    else
    {
        const uint8_t bankNumData[] = { 0, 1, 2, 3 };
        _busAccess.blockWrite(BANK_16K_BASE_ADDR, bankNumData, 4, false, true);
    }

    // Enable register outputs
    const uint8_t setRegEn[] = { 1 };
    _busAccess.blockWrite(BANK_16K_PAGE_ENABLE, setRegEn, 1, false, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read/Write to banked physical memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE HwRAMROM::readWriteBankedMemory(uint32_t addr, uint8_t* pBuf, uint32_t len,
            bool iorq, bool write)
{
    // Return value
    BR_RETURN_TYPE retVal = BR_OK;

    // Enable register outputs
    const uint8_t setRegEn[] = { 1 };
    _busAccess.blockWrite(BANK_16K_PAGE_ENABLE, setRegEn, 1, false, true);

    // Access memory using bank 0
    uint32_t initialBankOffset = addr % BANK_SIZE_BYTES;
    uint32_t num16KBanks = (len == 0) ? 0 : ((len - 1 + initialBankOffset) / BANK_SIZE_BYTES) + 1;
    uint8_t bankNumber = addr / BANK_SIZE_BYTES;
    uint32_t bytesRemaining = len;
    
    // Start address and initial page number
    for (uint32_t i = 0; i < num16KBanks; i++)
    {
        // Start and length calculation for first block
        uint32_t start = initialBankOffset;
        uint32_t lenInBank = (num16KBanks == 1) ? bytesRemaining : BANK_SIZE_BYTES - start;
        // Other blocks
        if (i > 0)
        {
            start = 0;
            lenInBank = (bytesRemaining < BANK_SIZE_BYTES) ? bytesRemaining : BANK_SIZE_BYTES;
        }
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "%s Addr %06x Len 0x%x NumBanks %d BankNo %x startAddr %04x lenInBank 0x%x", 
        //                 write ? "WRITE" : "READ", addr, len, num16KBanks, bankNumber, start, lenInBank);

        // Write the bank number to bank register 0
        const uint8_t bankNumData[] = { bankNumber };
        _busAccess.blockWrite(BANK_16K_BASE_ADDR, bankNumData, 1, false, true);

        // Perform the memory operation
        if (write)
            retVal = _busAccess.blockWrite(start, pBuf, lenInBank, false, iorq);
        else
            retVal = _busAccess.blockRead(start, pBuf, lenInBank, false, iorq);
        if (retVal != BR_OK)
            break;

        // Bump bank number
        bankNumber++;
        pBuf += lenInBank;
        bytesRemaining -= lenInBank;
    }

    // Disable memory registers
    const uint8_t setRegEnableValue[] = { _bankRegisterOutputEnable };
    _busAccess.blockWrite(BANK_16K_PAGE_ENABLE, setRegEnableValue, 1, false, true);

    // Write the current bank number back to bank register 0
    const uint8_t bankNumData[] = { _bankRegisters[0] };
    _busAccess.blockWrite(BANK_16K_BASE_ADDR, bankNumData, 1, false, true);
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handling of linear and banked physical memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE HwRAMROM::physicalBlockAccess(uint32_t addr, const uint8_t* pBuf, uint32_t len,
             bool busRqAndRelease, bool iorq, bool write)
{
    // Check if banked registers need to be changed
    // if linear
    //   if topAddress below 64K
    //     just access linearly
    //   else 
    //     switch into bank mode and use bank regs to access banks
    // else
    //   store bank regs, then use bank regs to access banks, restore bank regs

    // Card mode
    if (_memoryCardOpMode == MEM_CARD_OP_MODE_LINEAR)
    {
        if (addr + len <= 65536)
        {
            if (write)
                return _busAccess.blockWrite(addr, pBuf, len, busRqAndRelease, iorq);
            return _busAccess.blockRead(addr, const_cast<uint8_t*>(pBuf), len, busRqAndRelease, iorq);
        }
        else
        {
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Setting to paged, lastByte = %02x", pBuf[len-1]);

            // Check if we need to request bus
            if (busRqAndRelease) {
                // Request bus and take control after ack
                BR_RETURN_TYPE ret = _busAccess.controlRequestAndTake();
                if (ret != BR_OK)
                {
                    LogWrite(MODULE_PREFIX, LOG_DEBUG, "Failed to acquire bus");
                    return ret;     
                }
            }

            // Switch card to banked mode
            const uint8_t setBankedMode[] = { 1 };
            _busAccess.blockWrite(BANK_16K_LIN_TO_PAGE, setBankedMode, 1, false, true);

            // Read/Write the banked memory block
            BR_RETURN_TYPE retVal = readWriteBankedMemory(addr, const_cast<uint8_t*>(pBuf), len, iorq, write);

            // Switch card back to linear mode if required
            if ((_memCardOpts & MEM_OPT_STAY_BANKED) == 0)
            {
                const uint8_t clearBankedMode[] = { 0 };
                _busAccess.blockWrite(BANK_16K_LIN_TO_PAGE, clearBankedMode, 1, false, true);
                // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Restore to linear");
            }

            // Check if we need to release bus
            if (busRqAndRelease) {
                // release bus
                _busAccess.controlRelease();
            }
            return retVal;
        }
    }
    else
    {
        // Read/Write the banked memory block
        readWriteBankedMemory(addr, const_cast<uint8_t*>(pBuf), len, iorq, write);

        // Check if banks should be set to emulate 64K linear address space
        if ((_memCardOpts & MEM_OPT_EMULATE_LINEAR) || (_memCardOpts & MEM_OPT_EMULATE_LINEAR_UPPER))
            setBanksToEmulate64KAddrSpace(_memCardOpts & MEM_OPT_EMULATE_LINEAR_UPPER);
    }
    return BR_NOT_HANDLED;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE HwRAMROM::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len,
             bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
{
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "HwRAMROM::blockWrite");
    // Check forced mirror access
    if (!forceMirrorAccess)
    {
        // Access physical memory
        return physicalBlockAccess(addr, pBuf, len, busRqAndRelease, iorq, true);
    }

    // Check for memory request
    if (iorq)
        return BR_NOT_HANDLED;

    // Tracer memory
    uint8_t* pMirrorMemory = getMirrorMemory();
    if (!pMirrorMemory)
        return BR_ERR;

    // Valid address and length
    if (addr + len > _mirrorMemoryLen)
        len = _mirrorMemoryLen-addr;
    // Write
    if (len > 0)
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "HwRAMROM::blockWrite %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, len, pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
        memcopyfast(pMirrorMemory+addr, pBuf, len);
    }
    return BR_OK;
}

BR_RETURN_TYPE HwRAMROM::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, 
             bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
{
    // Check forced mirror access
    if (!forceMirrorAccess)
    {
        // Access physical memory
        return physicalBlockAccess(addr, pBuf, len, busRqAndRelease, iorq, false);
    }

    // Check for memory request
    if (iorq)
        return BR_NOT_HANDLED;

    // Access mirror memory
    uint8_t* pMirrorMemory = getMirrorMemory();
    if (!pMirrorMemory)
        return BR_ERR;

    // Valid address and length
    if (len > _mirrorMemoryLen)
        len = _mirrorMemoryLen;
    // First part len - in case of wrap
    size_t firstPartLen = len;
    if (addr + len > _mirrorMemoryLen)
    {
        // First part len
        firstPartLen = _mirrorMemoryLen-addr;
    }
    if (firstPartLen > 0)
    {
        memcopyfast(pBuf, pMirrorMemory+addr, firstPartLen);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "HwRAMROM::blockRead %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, firstPartLen, pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
    }
    // Check for wrap to get second section
    if (len > firstPartLen)
    {
        memcopyfast(pBuf+firstPartLen, pMirrorMemory, len-firstPartLen);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "HwRAMROM::blockRead wrap %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, len, pBuf[firstPartLen], pBuf[firstPartLen+1], pBuf[firstPartLen+2], pBuf[firstPartLen+3]);
    }
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mirror memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Get mirror memory for address
uint8_t* HwRAMROM::getMirrorMemForAddr(uint32_t addr)
{
    uint8_t* pMirrorMemory = getMirrorMemory();
    if (!pMirrorMemory)
        return pMirrorMemory;
    return pMirrorMemory + addr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracer interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HwRAMROM::tracerClone()
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "tracerClone emulated %d mem %d", 
            _isEmulatingMemory, getTracerMemory());

    // Tracer memory
    uint8_t* pValMemory = getTracerMemory();
    if (!pValMemory)
        return;

    // Check if in emulated memory mode
    if (_isEmulatingMemory)
    {
        uint8_t* pSrcMemory = getMirrorMemory();
        if (!pSrcMemory)
            return;
        uint32_t maxLen = _mirrorMemoryLen < _tracerMemoryLen ? _mirrorMemoryLen : _tracerMemoryLen;
        memcopyfast(pValMemory, pSrcMemory, maxLen);
    }
    else
    {
        // int blockReadResult = 
        _busAccess.blockRead(0, pValMemory, _tracerMemoryLen, false, false);
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "tracerClone blockRead %s %02x %02x %02x",
        //         (blockReadResult == BR_OK) ? "OK" : "FAIL",
        //         pValMemory[0], pValMemory[1], pValMemory[2]);
    }
}

void HwRAMROM::tracerHandleAccess(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    // Check tracer memory ok
    uint8_t* pMemory = getTracerMemory();
    if (!pMemory)
        return;

    // Memory requests use tracer memory
    if (flags & BR_CTRL_BUS_MREQ_MASK)
    {
        if (flags & BR_CTRL_BUS_WR_MASK)
            pMemory[addr] = data;
        else if (flags & BR_CTRL_BUS_RD_MASK)
            retVal = pMemory[addr];
    }
}

uint8_t* HwRAMROM::getTracerMemory()
{
    if (!_pTracerMemory)
    {
        _pTracerMemory = new uint8_t[_tracerMemoryLen];
        if (!_tracerMemAllocNotified)
        {
            _tracerMemAllocNotified = true;
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "Alloc for tracer memory len %d %s", 
                    _tracerMemoryLen, _pTracerMemory ? "OK" : "FAIL");
        }
    }

    return _pTracerMemory;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle a completed bus action
void HwRAMROM::handleBusActionActive(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason, 
            BR_RETURN_TYPE rslt)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busActionActive %d", actionType);
    if (rslt != BR_OK)
        return;
    switch(actionType)
    {
        case BR_BUS_ACTION_RESET:
            hwReset();
            // Request the bus again so we can change the mode of the card if required
            if ((_memCardOpts & MEM_OPT_STAY_BANKED) || (_memCardOpts & MEM_OPT_EMULATE_LINEAR) || (_memCardOpts & MEM_OPT_EMULATE_LINEAR_UPPER))
            // if (_memCardOpts & MEM_OPT_STAY_BANKED)
            {
                // LogWrite(MODULE_PREFIX, LOG_DEBUG, "BUSRQ Opts %02x stayBanked %d, emuLinear %d, emuLinearUpper %d", 
                //             _memCardOpts, 
                //             _memCardOpts & MEM_OPT_STAY_BANKED,
                //             _memCardOpts & MEM_OPT_EMULATE_LINEAR,
                //             _memCardOpts & MEM_OPT_EMULATE_LINEAR_UPPER
                //             );
                _busAccess.targetReqBus(_hwManager.getBusSocketId(), BR_BUS_ACTION_HW_ACTION);
            }
            break;
        case BR_BUS_ACTION_PAGE_OUT_FOR_INJECT:
            pageOutForInjection(true);
            break;
        case BR_BUS_ACTION_PAGE_IN_FOR_INJECT:
            pageOutForInjection(false);
            break;
        case BR_BUS_ACTION_BUSRQ:
            if (reason == BR_BUS_ACTION_MIRROR)
            {
                if (!_mirrorMode)
                    break;
                // Make a copy of the enire memory while we have the chance
                // int blockReadResult = 
                _busAccess.blockRead(0, getMirrorMemory(), _mirrorMemoryLen, false, false);
                // Debug
                // LogWrite(MODULE_PREFIX, LOG_DEBUG, "mirror memory blockRead %s addr %04x %d [0] %02x [1] %02x [2] %02x [3] %02x mirror %d %s",
                //              (blockReadResult == BR_OK) ? "OK" : "FAIL",
                //              0, _mirrorMemoryLen,
                //              getMirrorMemory()[0],
                //              getMirrorMemory()[1],
                //              getMirrorMemory()[2],
                //              getMirrorMemory()[3],
                //              _mirrorMode,
                //              _pName);
            }
            if (reason == BR_BUS_ACTION_HW_ACTION)
            {
                // Switch card back to banked mode if required
                if (_memCardOpts & MEM_OPT_STAY_BANKED)
                {
                    const uint8_t setBankedMode[] = { 1 };
                    _busAccess.blockWrite(BANK_16K_LIN_TO_PAGE, setBankedMode, 1, true, true);
                    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "HWAction Staying banked");
                }
                // Check if banks should be set to emulate 64K linear address space
                if ((_memCardOpts & MEM_OPT_EMULATE_LINEAR) || (_memCardOpts & MEM_OPT_EMULATE_LINEAR_UPPER))
                {
                    setBanksToEmulate64KAddrSpace(_memCardOpts & MEM_OPT_EMULATE_LINEAR_UPPER);
                    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "HWAction set to emulate 64K using %s 512K chip",
                    //             _memCardOpts & MEM_OPT_EMULATE_LINEAR_UPPER ? "upper" : "lower");
                }
            }
            break;
        default:
            break;
    }
}

// Handle a request for memory or IO - or possibly something like an interrupt vector in Z80
void HwRAMROM::handleMemOrIOReq( uint32_t addr,  uint32_t data, 
         uint32_t flags,  uint32_t& retVal)
{
    // Memory requests
    if (flags & BR_CTRL_BUS_MREQ_MASK)
    {
        // Check emulation mode
        if (_isEmulatingMemory || _mirrorMode)
        {
            // Check mirror memory ok
            uint8_t* pMemory = getMirrorMemory();
            if (!pMemory)
                return;

            if (flags & BR_CTRL_BUS_WR_MASK)
            {
                pMemory[addr] = data;
            }
            else if ((flags & BR_CTRL_BUS_RD_MASK) && (!_mirrorMode))
            {
                // In mirror mode only writes are handled - reads come from the systems memory
                retVal = (retVal & 0xffff0000) | pMemory[addr];
            }
        }
    }

    // Check for IO address range used by this card
    if (flags & BR_CTRL_BUS_IORQ_MASK)
    {
        uint32_t ioAddr = (addr & 0xff);
        if ((ioAddr >= _bankHwBaseIOAddr) && (ioAddr < _bankHwBaseIOAddr + NUM_BANKS))
        {
            if(flags & BR_CTRL_BUS_WR_MASK)
            {
                _bankRegisters[ioAddr - _bankHwBaseIOAddr] = data;
                // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_B + ioAddr - _bankHwBaseIOAddr, data);
            }
        }
        else if (ioAddr == _bankHwPageEnIOAddr)
        {
            if (flags & BR_CTRL_BUS_WR_MASK)
            {
                _bankRegisterOutputEnable = ((data & 0x01) != 0);
                // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_K, data);
            }
        }
    }
}
