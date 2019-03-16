// Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../TargetBus/TargetCPU.h"

class MemorySystem
{
public:
    MemorySystem(uint32_t memorySize);
    ~MemorySystem();

    // Memory access
    uint8_t getMemoryByte(uint32_t addr);
    void putMemoryByte(uint32_t addr, uint32_t data);
    uint16_t getMemoryWord(uint32_t addr);
    void clearMemory();
    bool blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len);
    bool blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len);

    // Direct access
    uint8_t* getMemBuffer();
    int getMemBufferLen();

private:

    // Target memory buffer
    uint8_t* _pTargetMemBuffer;
    uint32_t _targetMemSize;
    uint8_t* checkAlloc();
};
