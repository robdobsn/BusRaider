// Bus Raider Hardware Base Class
// Rob Dobson 2019

#include "HwBase.h"
#include "HwManager.h"
#include "BusAccess.h"

static const char* _baseName = "UNDEFINED";

HwBase::HwBase(HwManager& hwManager, BusAccess& busAccess) : 
        _hwManager(hwManager), _busAccess(busAccess)
{
    HwManager::addHardwareElementStatic(this);
    _enabled = false;
    _pName = _baseName;
}

// Page out RAM/ROM due to emulation
void HwBase::setMemoryEmulationMode( bool val)
{
}

// Page out RAM/ROM for opcode injection
void HwBase::pageOutForInjection( bool pageOut)
{
}

// Set paging enable
void HwBase::setMemoryPagingEnable( bool val)
{
}

// Set mirror mode
void HwBase::setMirrorMode( bool val)
{
}

// Mirror clone
void HwBase::mirrorClone()
{

}

// Handle a completed bus action
void HwBase::handleBusActionActive(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason, 
        BR_RETURN_TYPE rslt)
{
}

// Handle a request for memory or IO - or possibly something like in interrupt vector in Z80
void HwBase::handleMemOrIOReq( uint32_t addr,  uint32_t data, 
             uint32_t flags,  uint32_t& retVal)
{
}

// Block access to hardware
BR_RETURN_TYPE HwBase::blockWrite( uint32_t addr,  const uint8_t* pBuf, 
             uint32_t len,  bool busRqAndRelease, 
             bool iorq,  bool forceMirrorAccess)
{
    return BR_NOT_HANDLED;
}

BR_RETURN_TYPE HwBase::blockRead( uint32_t addr,  uint8_t* pBuf, 
             uint32_t len,  bool busRqAndRelease, 
             bool iorq,  bool forceMirrorAccess)
{
    return BR_NOT_HANDLED;
}

// Get mirror memory for address
uint8_t* HwBase::getMirrorMemForAddr(uint32_t addr)
{
    return NULL;
}

// Tracer interface to hardware
void HwBase::tracerClone()
{

}

void HwBase::tracerHandleAccess( uint32_t addr,  uint32_t data, 
         uint32_t flags,  uint32_t& retVal)
{
    
}

// Configure
void HwBase::configure( const char* jsonConfig)
{

}