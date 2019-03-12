// Bus Raider
// Rob Dobson 2019

#include "RAMEmulator.h"
#include <string.h>
#include <stdlib.h>
#include "../System/ee_printf.h"
#include "../TargetBus/BusAccess.h"
#include "../System/piwiring.h"
#include "../System/lowlib.h"
#include "../System/rdutils.h"

// Module name
static const char FromRAMEMU[] = "RAMEMU";

// Memory buffer
uint8_t RAMEmulator::_targetMemBuffer[STD_TARGET_MEMORY_LEN];

// Vars
bool RAMEmulator::_emulatedRAMReadPagingActive = false;
McBase* RAMEmulator::_pTargetMachine = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RAMEmulator::setup(McBase* pTargetMachine)
{
    _pTargetMachine = pTargetMachine;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t RAMEmulator::getMemoryByte(uint32_t addr)
{
    if (addr >= MAX_TARGET_MEM_ADDR)
        return 0;
    return _targetMemBuffer[addr];
}

uint16_t RAMEmulator::getMemoryWord(uint32_t addr)
{
    if (addr >= MAX_TARGET_MEM_ADDR)
        return 0;
    return _targetMemBuffer[(addr+1) % STD_TARGET_MEMORY_LEN] * 256 + _targetMemBuffer[addr];
}

void RAMEmulator::clearMemory()
{
    memset(_targetMemBuffer, 0, STD_TARGET_MEMORY_LEN);
}

bool RAMEmulator::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len)
{
    LogWrite(FromRAMEMU, LOG_DEBUG, "blockWrite to %04x len %d", addr, len);
    int copyLen = len;
    if (addr >= STD_TARGET_MEMORY_LEN)
        return false;
    if (addr + copyLen > STD_TARGET_MEMORY_LEN)
        copyLen = STD_TARGET_MEMORY_LEN - addr;
    memcpy(_targetMemBuffer + addr, pBuf, copyLen);
    return true;
}

bool RAMEmulator::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len)
{
    if (!((_pTargetMachine && _pTargetMachine->getDescriptorTable(0)->emulatedRAM &&
            (addr >= _pTargetMachine->getDescriptorTable(0)->emulatedRAMStart) && 
            (addr < _pTargetMachine->getDescriptorTable(0)->emulatedRAMStart + _pTargetMachine->getDescriptorTable(0)->emulatedRAMLen))))
    {
        // LogWrite(FromTargetDebug, LOG_VERBOSE, "blockRead: Not in single step or emulated RAM %04x %d", addr, len);
        return false;
    }
    int copyLen = len;
    if (addr >= STD_TARGET_MEMORY_LEN)
    {
        LogWrite(FromRAMEMU, LOG_DEBUG, "blockRead: Too long %04x %d", addr, len);
        return false;
    }
    if (addr + copyLen > STD_TARGET_MEMORY_LEN)
        copyLen = STD_TARGET_MEMORY_LEN - addr;
    memcpy(pBuf, _targetMemBuffer + addr, copyLen);
    return true;
}

uint8_t* RAMEmulator::getMemBuffer()
{
    return _targetMemBuffer;
}

int RAMEmulator::getMemBufferLen()
{
    return sizeof(_targetMemBuffer);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RAMEmulator::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RAMEmulator::handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal,
            [[maybe_unused]] McDescriptorTable& descriptorTable)
{
    // Check if RAM/ROM paged out from last MREQ
    if (_emulatedRAMReadPagingActive)
    {
        // Release paging on physical RAM/ROM
        digitalWrite(BR_PAGING_RAM_PIN, 0);
        _emulatedRAMReadPagingActive = false;
    }

    // Check if RAM is emulated
    if (descriptorTable.emulatedRAM && (addr >= descriptorTable.emulatedRAMStart) && 
            (addr < descriptorTable.emulatedRAMStart + descriptorTable.emulatedRAMLen))
    {
        // Check for read or write to emulated RAM / ROM
        if (flags & BR_CTRL_BUS_RD_MASK)
        {
            retVal = _targetMemBuffer[addr];
            // Page-out physical RAM/ROM
            digitalWrite(BR_PAGING_RAM_PIN, 1);
            _emulatedRAMReadPagingActive = true;

        }
        else if (flags & BR_CTRL_BUS_WR_MASK)
        {
            _targetMemBuffer[addr] = data;
        }
    }

    return retVal;
}

