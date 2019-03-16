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

// Vars
bool RAMEmulator::_emulationActive = false;
McBase* RAMEmulator::_pTargetMachine = NULL;
MemorySystem RAMEmulator::_memorySystem(STD_TARGET_MEMORY_LEN);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RAMEmulator::setup(McBase* pTargetMachine)
{
    _pTargetMachine = pTargetMachine;
}

void RAMEmulator::activateEmulation(bool active)
{
    _emulationActive = active;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t RAMEmulator::getMemoryByte(uint32_t addr)
{
    return _memorySystem.getMemoryByte(addr);
}

uint16_t RAMEmulator::getMemoryWord(uint32_t addr)
{
    return _memorySystem.getMemoryWord(addr);
}

void RAMEmulator::clearMemory()
{
    _memorySystem.clearMemory();
}

bool RAMEmulator::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len)
{
    return _memorySystem.blockWrite(addr, pBuf, len);
}

bool RAMEmulator::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len)
{
    return _memorySystem.blockRead(addr, pBuf, len);
}

uint8_t* RAMEmulator::getMemBuffer()
{
    return _memorySystem.getMemBuffer();
}

int RAMEmulator::getMemBufferLen()
{
    return _memorySystem.getMemBufferLen();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RAMEmulator::service()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt handler for Wait-States
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RAMEmulator::handleWaitInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    // Check if RAM is emulated and MREQ
    if (_emulationActive && (flags & BR_CTRL_BUS_MREQ_MASK))
    {
        // Check for read or write to emulated RAM / ROM
        if (flags & BR_CTRL_BUS_RD_MASK)
        {
            retVal = _memorySystem.getMemoryByte(addr);
        }
        else if (flags & BR_CTRL_BUS_WR_MASK)
        {
            _memorySystem.putMemoryByte(addr, data);
        }
    }

    return retVal;
}

