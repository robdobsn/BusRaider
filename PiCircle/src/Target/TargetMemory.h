// Pi Bus Raider
// Rob Dobson 2018

#pragma once
#include <stdint.h>

class TargetMemoryBlock 
{
public:
    uint32_t start;
    uint32_t len;
};

class TargetMemory 
{
public:
    void clear();
    void dataBlockStore(uint32_t addr, const uint8_t* pData, uint32_t len);
    int getNumBlocks();
    TargetMemoryBlock* getMemoryBlock(int n);
    unsigned char* memoryPtr();

private:
    static const int MAX_TARGET_MEMORY_BLOCKS = 200;
    static const int MAX_TARGET_MEMORY_SIZE = 0x10000;
    uint8_t _pTargetBuffer[MAX_TARGET_MEMORY_SIZE];
    TargetMemoryBlock _memoryBlocks[MAX_TARGET_MEMORY_BLOCKS];
    int _memoryBlockLastIdx;
};


