// Bus Raider
// Rob Dobson 2018-2020

#include "MemoryInterface.h"
#include "BusControl.h"

// Module name
static const char MODULE_PREFIX[] = "MemCtrl";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemoryInterface::MemoryInterface(CPUHandler& cpuHandler)
    : _cpuHandler(cpuHandler)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MemoryInterface::clear()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block access
// Read a consecutive block of memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE MemoryInterface::blockRead(uint32_t addr, uint8_t* pData, 
            uint32_t len, BlockAccessType accessType)
{
    if (_cpuHandler.isDebugging())
        return memCacheBlockRead(addr, pData, len, accessType);
    return _cpuHandler.blockRead(addr, pData, len, accessType);
}

// Write a consecutive block of memory to host
BR_RETURN_TYPE MemoryInterface::blockWrite(uint32_t addr, const uint8_t* pData, 
            uint32_t len, BlockAccessType accessType)
{
    if (_cpuHandler.isDebugging())
        return memCacheBlockWrite(addr, pData, len, accessType);
    return _cpuHandler.blockWrite(addr, pData, len, accessType);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory cache block access
// Read a consecutive block of cache memory
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE MemoryInterface::memCacheBlockRead(uint32_t addr, uint8_t* pData, 
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

BR_RETURN_TYPE MemoryInterface::memCacheBlockWrite(uint32_t addr, const uint8_t* pData, 
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

