// Bus Raider Hardware RC2014 64K RAM
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

const char* Hw64KRam::_logPrefix = "RC2014_64K_RAM";
const char* Hw64KRam::_baseName = "64KRAM";

Hw64KRam::Hw64KRam() : HwBase()
{
    _mirrorMemoryLen = 64*1024;
    _pMirrorMemory = NULL;
    _tracerMemoryLen = _mirrorMemoryLen;
    _pTracerMemory = NULL;
    _pName = _baseName;
    _memoryEmulationMode = false;
    hwReset();
}

// Set memory emulation mode - page out real RAM/ROM and use mirror memory
// for bus accesses
void Hw64KRam::setMemoryEmulationMode(bool pageOut)
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
void Hw64KRam::pageOutForInjection(bool pageOut)
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
void Hw64KRam::hwReset()
{
    _pagingEnabled = true;
    _currentlyPagedOut = false;
    BusAccess::busPagePinSetActive(false);
}

// Mirror mode
void Hw64KRam::setMirrorMode(bool val)
{
    // LogWrite(_logPrefix, LOG_DEBUG, "Mirror mode %d", val);
    _mirrorMode = val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mirror memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t* Hw64KRam::getMirrorMemory()
{
    if (!_pMirrorMemory)
        _pMirrorMemory = new uint8_t[_mirrorMemoryLen];
    return _pMirrorMemory;
}

void Hw64KRam::mirrorClone()
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

BR_RETURN_TYPE Hw64KRam::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len,
            [[maybe_unused]] bool busRqAndRelease, bool iorq, bool forceMirrorAccess)
{
    //     LogWrite(_logPrefix, LOG_DEBUG, "Hw64KRam::blockWrite");
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
        // LogWrite(_logPrefix, LOG_DEBUG, "Hw64KRam::blockWrite %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, len, pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
        memcopyfast(pMirrorMemory+addr, pBuf, len);
    }
    return BR_OK;
}

BR_RETURN_TYPE Hw64KRam::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, 
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

    // Tracer memory
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
        // LogWrite(_logPrefix, LOG_DEBUG, "Hw64KRam::blockRead %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, firstPartLen, pBuf[0], pBuf[1], pBuf[2], pBuf[3]);
    }
    // Check for wrap to get second section
    if (len > firstPartLen)
    {
        memcopyfast(pBuf+firstPartLen, pMirrorMemory, len-firstPartLen);
        // LogWrite(_logPrefix, LOG_DEBUG, "Hw64KRam::blockRead wrap %04x %d [0] %02x [1] %02x [2] %02x [3] %02x",
        //         addr, len, pBuf[firstPartLen], pBuf[firstPartLen+1], pBuf[firstPartLen+2], pBuf[firstPartLen+3]);
    }
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mirror memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Get mirror memory for address
uint8_t* Hw64KRam::getMirrorMemForAddr(uint32_t addr)
{
    uint8_t* pMirrorMemory = getMirrorMemory();
    if (!pMirrorMemory)
        return pMirrorMemory;
    return pMirrorMemory + addr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracer interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Hw64KRam::tracerClone()
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

void Hw64KRam::tracerHandleAccess(uint32_t addr, uint32_t data, 
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

uint8_t* Hw64KRam::getTracerMemory()
{
    if (!_pTracerMemory)
        _pTracerMemory = new uint8_t[_tracerMemoryLen];
    return _pTracerMemory;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks/Hooks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle a completed bus action
void Hw64KRam::handleBusActionComplete([[maybe_unused]]BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
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
void Hw64KRam::handleMemOrIOReq([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
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
}
