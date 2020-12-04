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
    // Check params
    if (addr >= STD_TARGET_MEMORY_LEN)
        return BR_ERR;
    uint32_t toCopy = len;
    if (addr + toCopy > STD_TARGET_MEMORY_LEN)
        toCopy = STD_TARGET_MEMORY_LEN - addr;

    // Copy
    memcpy(pData, _memCache+addr, toCopy);
    return BR_OK;
}

BR_RETURN_TYPE MemoryController::memCacheBlockWrite(uint32_t addr, const uint8_t* pData, 
            uint32_t len, BlockAccessType accessType)
{
    // Check params
    if (addr >= STD_TARGET_MEMORY_LEN)
        return BR_ERR;
    uint32_t toCopy = len;
    if (addr + toCopy > STD_TARGET_MEMORY_LEN)
        toCopy = STD_TARGET_MEMORY_LEN - addr;

    // Copy
    memcpy(_memCache+addr, pData, toCopy);
    return BR_OK;
}

