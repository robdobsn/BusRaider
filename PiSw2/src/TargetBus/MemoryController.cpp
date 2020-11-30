// Bus Raider
// Rob Dobson 2018-2020

#include "MemoryController.h"
#include "BusControl.h"

// Module name
static const char MODULE_PREFIX[] = "MemCtrl";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemoryController::MemoryController(BusControl& busControl)
    : _busControl(busControl)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryController::clear()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block access
// Read a consecutive block of memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE MemoryController::blockRead(uint32_t addr, uint8_t* pData, 
            uint32_t len, BlockAccessType accessType)
{
    if (_busControl.isDebugging())
        return memCacheBlockRead(addr, pData, len, accessType);
    return _busControl.bus().rawBlockRead(addr, pData, len, accessType);
}

// Write a consecutive block of memory to host
BR_RETURN_TYPE MemoryController::blockWrite(uint32_t addr, const uint8_t* pData, 
            uint32_t len, BlockAccessType accessType)
{
    if (_busControl.isDebugging())
        return memCacheBlockWrite(addr, pData, len, accessType);
    return _busControl.bus().rawBlockWrite(addr, pData, len, accessType);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory cache block access
// Read a consecutive block of cache memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE MemoryController::memCacheBlockRead(uint32_t addr, uint8_t* pData, 
            uint32_t len, BlockAccessType accessType)
{
    // TODO 2020 implement
    return BR_ERR;
}

BR_RETURN_TYPE MemoryController::memCacheBlockWrite(uint32_t addr, const uint8_t* pData, 
            uint32_t len, BlockAccessType accessType)
{
    // TODO 2020 implement
    return BR_ERR;
}

