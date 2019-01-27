// Pi Bus Raider
// Rob Dobson 2018-2019

#include "targetMemory.h"


void TargetMemory::clear()
{
    _memoryBlockLastIdx = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_BLOCKS; i++)
        _memoryBlocks[i].len = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_SIZE; i++)
        _pTargetBuffer[i] = 0;
}

void TargetMemory::dataBlockStore(uint32_t addr, const uint8_t* pData, uint32_t len)
{
    // Check if contiguous with other data
    int blockIdx = -1;
    for (int i = 0; i < _memoryBlockLastIdx; i++) {
        if (_memoryBlocks[i].start + _memoryBlocks[i].len == addr) {
            blockIdx = i;
            _memoryBlocks[i].len += len;
            break;
        }
    }

    // New block
    if (blockIdx == -1) {
        if (_memoryBlockLastIdx >= MAX_TARGET_MEMORY_BLOCKS) {
            // ee_printf("Too many target memory blocks\n");
            return;
        }
        _memoryBlocks[_memoryBlockLastIdx].start = addr;
        _memoryBlocks[_memoryBlockLastIdx].len = len;
        _memoryBlockLastIdx++;
    }

    // ee_printf("Blk st %04x len %d\n", addr, len);
    
    // Store block
    for (unsigned int i = 0; i < len; i++) {
        if (addr + i < MAX_TARGET_MEMORY_SIZE)
            _pTargetBuffer[addr + i] = pData[i];
    }
}

int TargetMemory::getNumBlocks()
{
    return _memoryBlockLastIdx;
}

TargetMemoryBlock* TargetMemory::getMemoryBlock(int n)
{
    return &_memoryBlocks[n];
}

unsigned char* TargetMemory::memoryPtr()
{
    return _pTargetBuffer;
}

