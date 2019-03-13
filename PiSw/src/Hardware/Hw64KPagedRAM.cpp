// Bus Raider Hardware RC2014 64K RAM
// Rob Dobson 2019

#include "Hw64KPagedRam.h"
#include "../TargetBus/BusAccess.h"
#include "../TargetBus/TargetState.h"
#include "../System/rdutils.h"
#include "../System/piwiring.h"
#include "../System/ee_printf.h"
#include "../TargetBus/TargetDebug.h"
#include <stdlib.h>
#include <string.h>

const char* Hw64KPagedRam::_logPrefix = "RC2014_64K_RAM";

Hw64KPagedRam::Hw64KPagedRam() : HwBase()
{
    reset();
}

// Reset has occurred
void Hw64KPagedRam::reset()
{
    _pageOutForEmulation = false;
    _pagingEnabled = true;
    _currentlyPagedOut = false;
}

// Page out RAM/ROM due to emulation
void Hw64KPagedRam::pageOutForEmulation(bool pageOut)
{
    _pageOutForEmulation = pageOut;

    // Paging
    if (!_pagingEnabled)
        return;

    // Debug
    LogWrite(_logPrefix, LOG_DEBUG, "pageOutForEmulation %s", pageOut ? "Y" : "N");

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
void Hw64KPagedRam::pageOutForInjection(bool pageOut)
{
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
        if (!_pageOutForEmulation)
            digitalWrite(BR_PAGING_RAM_PIN, 0);
    }
}

// Check if paging requires bus access
bool Hw64KPagedRam::pagingRequiresBusAccess()
{
    return false;
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t Hw64KPagedRam::handleMemOrIOReq([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    return retVal;
}
