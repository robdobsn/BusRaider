// Bus Raider Hardware RC2014 64K RAM
// Rob Dobson 2019

#include "Hw64KRam.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../System/rdutils.h"
#include "../System/piwiring.h"
#include "../System/logging.h"
#include <stdlib.h>
#include <string.h>

const char* Hw64KRam::_logPrefix = "RC2014_64K_RAM";
const char* Hw64KRam::_baseName = "64KRAM";

Hw64KRam::Hw64KRam() : HwBase()
{
    _mirrorMemoryLen = 64*1024;
    _pMirrorMemory = NULL;
    _validatorMemoryLen = _mirrorMemoryLen;
    _pValidatorMemory = NULL;
    hwReset();
    strlcat(_name, _baseName, MAX_HW_NAME_LEN);
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
        digitalWrite(BR_PAGING_RAM_PIN, 1);
    }
    else
    {
        // Release paging unless otherwise paged out
        if (!_currentlyPagedOut)
            digitalWrite(BR_PAGING_RAM_PIN, 0);
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
        digitalWrite(BR_PAGING_RAM_PIN, 1);
    }
    else
    {
        // Release paging unless emulation in progress
        if (!_memoryEmulationMode)
            digitalWrite(BR_PAGING_RAM_PIN, 0);
    }
}

// Hardware reset has occurred
void Hw64KRam::hwReset()
{
    _memoryEmulationMode = false;
    _pagingEnabled = true;
    _currentlyPagedOut = false;
    digitalWrite(BR_PAGING_RAM_PIN, 0);
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE Hw64KRam::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len,
            [[maybe_unused]] bool busRqAndRelease, bool iorq)
{
    // Check for memory request
    if (iorq)
        return BR_NOT_HANDLED;

    // Validator memory
    uint8_t* pMirrorMemory = getMirrorMemory();
    if (!pMirrorMemory)
        return BR_ERR;

    // Valid address and length
    if (addr + len > _mirrorMemoryLen)
        len = _mirrorMemoryLen-addr;
    // Write
    if (len > 0)
        memcpy(pMirrorMemory, pBuf, len);
    return BR_OK;
}

BR_RETURN_TYPE Hw64KRam::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len, 
            [[maybe_unused]] bool busRqAndRelease, bool iorq)
{
    // Check for memory request
    if (iorq)
        return BR_NOT_HANDLED;

    // Validator memory
    uint8_t* pMirrorMemory = getMirrorMemory();
    if (!pMirrorMemory)
        return BR_ERR;

    // Valid address and length
    if (addr + len > _mirrorMemoryLen)
        len = _mirrorMemoryLen-addr;
    // Write
    if (len > 0)
        memcpy(pBuf, pMirrorMemory, len);
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Validator interface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Hw64KRam::validatorClone()
{
    // Validator memory
    uint8_t* pValMemory = getValidatorMemory();
    if (!pValMemory)
        return;

    // Check if in emulated memory mode
    if (_memoryEmulationMode)
    {
        uint8_t* pSrcMemory = getMirrorMemory();
        if (!pSrcMemory)
            return;
        uint32_t maxLen = _mirrorMemoryLen < _validatorMemoryLen ? _mirrorMemoryLen : _validatorMemoryLen;
        memcpy(pValMemory, pSrcMemory, maxLen);
    }
    else
    {
        int blockReadResult = BusAccess::blockRead(0, pValMemory, _validatorMemoryLen, true, false);
        LogWrite(_logPrefix, LOG_DEBUG, "validatorClone blockRead %s", (blockReadResult == BR_OK) ? "OK" : "FAIL");
        LogWrite(_logPrefix, LOG_DEBUG, "validatorClone blockRead %02x %02x %02x", pValMemory[0], pValMemory[1], pValMemory[2]);
    }
}

void Hw64KRam::validatorHandleAccess(uint32_t addr, uint32_t data, 
        uint32_t flags, uint32_t& retVal)
{
    // Check validator memory ok
    uint8_t* pMemory = getValidatorMemory();
    if (!pMemory)
        return;

    // Memory requests use validator memory
    if (flags & BR_CTRL_BUS_MREQ_MASK)
    {
        if (flags & BR_CTRL_BUS_WR_MASK)
            pMemory[addr] = data;
        else if (flags & BR_CTRL_BUS_RD_MASK)
            retVal = pMemory[addr];
    }
}

uint8_t* Hw64KRam::getValidatorMemory()
{
    if (!_pValidatorMemory)
        _pValidatorMemory = new uint8_t[_validatorMemoryLen];
    return _pValidatorMemory;
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
                // Make a copy of the enire memory while we have the chance
                // int blockReadResult = 
                BusAccess::blockRead(0, getMirrorMemory(), _mirrorMemoryLen, false, false);
                // LogWrite(_logPrefix, LOG_DEBUG, "mirror memory blockRead %s", (blockReadResult == BR_OK) ? "OK" : "FAIL");
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
    if (_memoryEmulationMode)
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
