// Bus Raider Hardware Base Class
// Rob Dobson 2019

#include "HwBase.h"
#include "HwManager.h"
#include "../TargetBus/BusAccess.h"

HwBase::HwBase()
{
    HwManager::add(this);
}

// Reset
void HwBase::reset()
{
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t HwBase::handleMemOrIOReq([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal)
{
    return retVal;
}

// Page out RAM/ROM due to emulation
void HwBase::pageOutForEmulation([[maybe_unused]] bool pageOut)
{
}

// Page out RAM/ROM for opcode injection
void HwBase::pageOutForInjection([[maybe_unused]] bool pageOut)
{
}

// Check if paging requires bus access
bool HwBase::pagingRequiresBusAccess()
{
    return false;
}

