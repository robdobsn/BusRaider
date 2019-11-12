// Bus Raider Hardware RC2014 RAMROM
// Rob Dobson 2019

#include "HwRAMROM.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../System/rdutils.h"
#include "../System/PiWiring.h"
#include "../System/lowlib.h"
#include "../System/logging.h"
#include <stdlib.h>
#include <string.h>

const char* HwRAMROM::_logPrefix = "HWRAMROM";
const char* HwRAMROM::_baseName = "RAMROM";

HwRAMROM::HwRAMROM() : HwBase()
{
    _memCardSizeBytes = 64*1024;
    _mirrorMemoryLen = _memCardSizeBytes;
    _pMirrorMemory = NULL;
    _mirrorMemAllocNotified = false;
    _tracerMemoryLen = _memCardSizeBytes;
    _pTracerMemory = NULL;
    _tracerMemAllocNotified = false;
    _pName = _baseName;
    _memoryEmulationMode = false;
    _memoryCardOpMode = MEM_CARD_OP_MODE_LINEAR;
    _bankHwBaseIOAddr = BANK_16K_BASE_ADDR;
    _bankHwPageEnIOAddr = BANK_16K_PAGE_ENABLE;
    hwReset();
}

// Configure
void HwRAMROM::configure([[maybe_unused]] const char* jsonConfig)
{
    LogWrite(_logPrefix, LOG_DEBUG, "configure %s", jsonConfig);

    uint32_t newMemSizeBytes = 64*1024;
    if (_memCardSizeBytes != newMemSizeBytes)
    {
        _memCardSizeBytes = newMemSizeBytes;
        _mirrorMemoryLen = _memCardSizeBytes;
        _tracerMemoryLen = _memCardSizeBytes;
        if (_pMirrorMemory)
        {
            delete [] _pMirrorMemory;
            _pMirrorMemory = NULL;
        }
        if (_pTracerMemory)
        {
            delete [] _pTracerMemory;
            _pTracerMemory = NULL;
        }
    }
}

// Set memory emulation mode - page out real RAM/ROM and use mirror memory
// for bus accesses
void HwRAMROM::setMemoryEmulationMode(bool pageOut)
{
    _memoryEmulationMode = pageOut;

    // Paging
    if (!_pagingEnabled)
        return;

    // Debug
    LogWrite(_logPrefix, LOG_DEBUG, "setMemoryEmulationMode %s", pageOut ? "Y" : "N");

    // Page out
    if (pageOut)
    {
        // Page out
        BusAccess::busPagePinSetActive(true);
    }
    else
    {
        // Release paging unless otherwise paged out
        if (!_currentlyPagedOut)
            BusAccess::busPagePinSetActive(false);
    }
}

// Page out RAM/ROM for opcode injection
void HwRAMROM::pageOutForInjection(bool pageOut)
{
    _currentlyPagedOut = pageOut;

    // LogWrite(_logPrefix, LOG_DEBUG, "pageOutForInjection %s pagingEn %d", pageOut ? "Y" : "N", _pagingEnabled);

    // Check enabled
    if (!_pagingEnabled)
        return;
        
    // Page out
    if (pageOut)
    {
        BusAccess::busPagePinSetActive(true);
    }
    else
    {
        // Release paging unless emulation in progress
        if (!_memoryEmulationMode)
            BusAccess::busPagePinSetActive(false);
    }
}

// Hardware reset has occurred
void HwRAMROM::hwReset()
{
    _pagingEnabled = true;
    _currentlyPagedOut = false;
    BusAccess::busPagePinSetActive(false);
    for (int i = 0; i < NUM_BANKS; i++)
        _bankRegisters[i] = 0;
}

// Mirror mode
void HwRAMROM::setMirrorMode(bool val)
{
    // LogWrite(_logPrefix, LOG_DEBUG, "Mirror mode %d", val);
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
            LogWrite(_logPrefix, LOG_DEBUG, "Alloc for mirror memory len %d %s", 
                    _mirrorMemoryLen, _pMirrorMemory ? "OK" : "FAIL");
        }
    }
    return _pMirrorMemory;
}

void HwRAMROM::mirrorClone()
{
    // No point doing this in emulator mode as mirror and emulator memory is shared
    if (_memoryEmulationMode)
        return;

    // Dest memory
    uint8_t* pDestMemory = getMirrorMemory();
    if (!pDestMemory)
        return;

    // int blockReadResult = 
    BusAccess::blockRead(0, pDestMemory, _mirrorMemoryLen, false, false);
    // LogWrite(_logPrefix, LOG_DEBUG, "mirrorClone blockRead %s %02x %02x %02x",
    //             (blockReadResult == BR_OK) ? "OK" : "FAIL",
    //             pDestMemory[0], pDestMemory[1], pDestMemory[2]);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE HwRAMROM::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len,
            [[maybe_unused]] bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
{
    //     LogWrite(_logPrefix, LOG_DEBUG, "HwRAMROM::blockWrite");
    // Check forced mirror access
    if (!forceMirrorAccess)
    {
        // Access memory via BusAccess
        return BusAccess::blockWrite(addr, pBuf, len, busRqAndRelease, iorq);
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
        // LogWrite(_logPrefix, LOG_DEBUG, "HwRAMROM::blockWrite %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, len, pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
        memcopyfast(pMirrorMemory+addr, pBuf, len);
    }
    return BR_OK;
}

BR_RETURN_TYPE HwRAMROM::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, 
            [[maybe_unused]] bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
{
    // Check forced mirror access
    if (!forceMirrorAccess)
    {
        // Access memory via BusAccess
        return BusAccess::blockRead(addr, pBuf, len, busRqAndRelease, iorq);
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
        // LogWrite(_logPrefix, LOG_DEBUG, "HwRAMROM::blockRead %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, firstPartLen, pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
    }
    // Check for wrap to get second section
    if (len > firstPartLen)
    {
        memcopyfast(pBuf+firstPartLen, pMirrorMemory, len-firstPartLen);
        // LogWrite(_logPrefix, LOG_DEBUG, "HwRAMROM::blockRead wrap %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
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
    LogWrite(_logPrefix, LOG_DEBUG, "tracerClone emulated %d mem %d", 
            _memoryEmulationMode, getTracerMemory());

    // Tracer memory
    uint8_t* pValMemory = getTracerMemory();
    if (!pValMemory)
        return;

    // Check if in emulated memory mode
    if (_memoryEmulationMode)
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
        BusAccess::blockRead(0, pValMemory, _tracerMemoryLen, false, false);
        // LogWrite(_logPrefix, LOG_DEBUG, "tracerClone blockRead %s %02x %02x %02x",
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
            LogWrite(_logPrefix, LOG_DEBUG, "Alloc for tracer memory len %d %s", 
                    _tracerMemoryLen, _pTracerMemory ? "OK" : "FAIL");
        }
    }

    return _pTracerMemory;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle a completed bus action
void HwRAMROM::handleBusActionComplete([[maybe_unused]]BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
{
    // LogWrite(_logPrefix, LOG_DEBUG, "busActionComplete %d", actionType);

    switch(actionType)
    {
        case BR_BUS_ACTION_RESET:
            hwReset();
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
                BusAccess::blockRead(0, getMirrorMemory(), _mirrorMemoryLen, false, false);
                // Debug
                // LogWrite(_logPrefix, LOG_DEBUG, "mirror memory blockRead %s addr %04x %d [0] %02x [1] %02x [2] %02x [3] %02x mirror %d %s",
                //              (blockReadResult == BR_OK) ? "OK" : "FAIL",
                //              0, _mirrorMemoryLen,
                //              getMirrorMemory()[0],
                //              getMirrorMemory()[1],
                //              getMirrorMemory()[2],
                //              getMirrorMemory()[3],
                //              _mirrorMode,
                //              _pName);
            }
            break;
        default:
            break;
    }
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void HwRAMROM::handleMemOrIOReq([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
    // Check emulation mode
    if (_memoryEmulationMode || _mirrorMode)
    {
        // Check mirror memory ok
        uint8_t* pMemory = getMirrorMemory();
        if (!pMemory)
            return;

        // Memory requests use mirror memory
        if (flags & BR_CTRL_BUS_MREQ_MASK)
        {
            if (flags & BR_CTRL_BUS_WR_MASK)
                pMemory[addr] = data;
            else if (flags & BR_CTRL_BUS_RD_MASK)
                retVal = (retVal & 0xffff0000) | pMemory[addr];
        }
    }

    // // Check for address range used by this card
    // if (_pagingEnabled && ((addr & 0xff) >= _bankHwBaseIOAddr) && ((addr & 0xff) < _bankHwBaseIOAddr + NUM_BANKS))
    // {
    //     if(flags & BR_CTRL_BUS_WR_MASK)
    //     {
    //         _bankRegisters[(addr & 0xff) - _bankHwBaseIOAddr] = data;
    //         //TODO
    //         // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_B + (addr & 0xff) - _bankHwBaseIOAddr, data);
    //     }
    // }
    // else if ((addr & 0xff) == _bankHwPageEnIOAddr)
    // {
    //     if (flags & BR_CTRL_BUS_WR_MASK)
    //     {
    //         _pagingEnabled = ((data & 0x01) != 0);
    //     }
    // }
}
