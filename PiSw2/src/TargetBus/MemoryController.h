// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include "lowlib.h"
#include "lowlev.h"
#include "BusSocketInfo.h"

class BusControl;

// Block access
enum BlockAccessType
{
    BLOCK_ACCESS_MEM,
    BLOCK_ACCESS_IO
};

class MemoryController
{
public:
    MemoryController(BusControl& busControl);

    // Clear
    void clear();

    // Read and write blocks
    BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, BlockAccessType accessType);
    BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pData, uint32_t len, BlockAccessType accessType);

    // Read and write cache memory blocks
    BR_RETURN_TYPE memCacheBlockRead(uint32_t addr, uint8_t* pData, 
            uint32_t len, BlockAccessType accessType);
    BR_RETURN_TYPE memCacheBlockWrite(uint32_t addr, const uint8_t* pData, 
            uint32_t len, BlockAccessType accessType);

    // Access to cache memory
    uint8_t* getCacheMemPtr()
    {
        return _memCache;
    }

private:
    // Bus control
    BusControl& _busControl;

    // Memory cache
    uint8_t _memCache[STD_TARGET_MEMORY_LEN];
};