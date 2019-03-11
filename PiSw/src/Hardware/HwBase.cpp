// Bus Raider Hardware Base Class
// Rob Dobson 2019

#include "HwBase.h"
#include "HwManager.h"
#include "../TargetBus/BusAccess.h"

HwBase::HwBase()
{
    HwManager::add(this);
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
uint32_t HwBase::handleMemOrIOReq([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, [[maybe_unused]] uint32_t flags)
{
    return BR_MEM_ACCESS_RSLT_NOT_DECODED;
}

// Page out RAM/ROM for opcode injection
void HwBase::pageOutRamRom([[maybe_unused]] bool restore)
{
}

// Check if paging requires bus access
bool HwBase::pagingRequiresBusAccess()
{
    return false;
}

