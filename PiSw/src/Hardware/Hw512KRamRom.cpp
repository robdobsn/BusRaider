// Bus Raider Hardware RC2014 512K RAM/ROM
// Rob Dobson 2019

#include "Hw512KRamRom.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../System/rdutils.h"
#include "../System/ee_printf.h"
#include "../Debugger/TargetDebug.h"
#include <stdlib.h>
#include <string.h>

const char* Hw512KRamRom::_logPrefix = "RC2014_512K_RAM_ROM";

static const int Hw512KRamRom_BASE_ADDR = 0x78;
static const int Hw512KRamRom_PAGE_ENABLE = 0x7c;

// Value to write to 74ls670 registers to disable the bank
// Actually any value with bit 5 set should work
static const int Hw512KRamRom_BANK_DISABLE = 0xff;

uint8_t Hw512KRamRom::_pageOutAllBanks[] = {
    Hw512KRamRom_BANK_DISABLE,
    Hw512KRamRom_BANK_DISABLE,
    Hw512KRamRom_BANK_DISABLE,
    Hw512KRamRom_BANK_DISABLE
};

Hw512KRamRom::Hw512KRamRom() : HwBase()
{
    _pagingEnabled = true;
    for (int i = 0; i < NUM_BANKS; i++)
        _bankRegisters[i] = 0;
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t Hw512KRamRom::handleMemOrIOReq([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;

    ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_A);

    // Check for address range used by this card
    if (_pagingEnabled && ((addr & 0xff) >= Hw512KRamRom_BASE_ADDR) && ((addr & 0xff) < Hw512KRamRom_BASE_ADDR + NUM_BANKS))
    {
        if(flags & BR_CTRL_BUS_WR_MASK)
        {
            _bankRegisters[(addr & 0xff) - Hw512KRamRom_BASE_ADDR] = data;
            ISR_VALUE(ISR_ASSERT_CODE_DEBUG_B + (addr & 0xff) - Hw512KRamRom_BASE_ADDR, data);
        }
    }
    else if ((addr & 0xff) == Hw512KRamRom_PAGE_ENABLE)
    {
        if (flags & BR_CTRL_BUS_WR_MASK)
        {
            _pagingEnabled = ((data & 0x01) != 0);
        }
    }

    // Not decoded
    return retVal;
}

// Page out RAM/ROM for opcode injection
void Hw512KRamRom::pageOutRamRom(bool restore)
{
    // Check enabled
    if (!_pagingEnabled)
        return;

    // Check page out or restore
    if (!restore)
    {
        // Write pageOut value
        BusAccess::blockWrite(Hw512KRamRom_BASE_ADDR, _pageOutAllBanks,
                NUM_BANKS, false, true);
    }
    else
    {
        // Restore
        BusAccess::blockWrite(Hw512KRamRom_BASE_ADDR, _bankRegisters,
                NUM_BANKS, false, true);
    }
}

// Check if paging requires bus access
bool Hw512KRamRom::pagingRequiresBusAccess()
{
    return _pagingEnabled;
}

