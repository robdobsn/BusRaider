// Bus Raider Hardware Base Class
// Rob Dobson 2019

#include "HwBase.h"
#include "HwManager.h"
#include "../TargetBus/BusAccess.h"

static const char* _baseName = "UNDEFINED";

HwBase::HwBase()
{
    HwManager::add(this);
    _enabled = false;
    _pName = _baseName;
}

// Page out RAM/ROM due to emulation
void HwBase::setMemoryEmulationMode([[maybe_unused]] bool val)
{
}

// Page out RAM/ROM for opcode injection
void HwBase::pageOutForInjection([[maybe_unused]] bool pageOut)
{
}

// Set paging enable
void HwBase::setMemoryPagingEnable([[maybe_unused]] bool val)
{
}

// Set mirror mode
void HwBase::setMirrorMode([[maybe_unused]] bool val)
{
}

// Mirror clone
void HwBase::mirrorClone()
{

}

// Handle a completed bus action
void HwBase::handleBusActionComplete([[maybe_unused]]BR_BUS_ACTION actionType, [[maybe_unused]] BR_BUS_ACTION_REASON reason)
{
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void HwBase::handleMemOrIOReq([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
}

// Block access to hardware
BR_RETURN_TYPE HwBase::blockWrite([[maybe_unused]] uint32_t addr, [[maybe_unused]] const uint8_t* pBuf, 
            [[maybe_unused]] uint32_t len, [[maybe_unused]] bool busRqAndRelease, 
            [[maybe_unused]] bool iorq, [[maybe_unused]] bool forceMirrorAccess)
{
    return BR_NOT_HANDLED;
}

BR_RETURN_TYPE HwBase::blockRead([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint8_t* pBuf, 
            [[maybe_unused]] uint32_t len, [[maybe_unused]] bool busRqAndRelease, 
            [[maybe_unused]] bool iorq, [[maybe_unused]] bool forceMirrorAccess)
{
    return BR_NOT_HANDLED;
}

// Get mirror memory for address
uint8_t* HwBase::getMirrorMemForAddr([[maybe_unused]]uint32_t addr)
{
    return NULL;
}

// Tracer interface to hardware
void HwBase::tracerClone()
{

}

void HwBase::tracerHandleAccess([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
        [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t& retVal)
{
    
}

// Configure
void HwBase::configure([[maybe_unused]] const char* name, [[maybe_unused]] const char* jsonConfig)
{

}