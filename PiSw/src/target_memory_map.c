// Pi Bus Raider
// Rob Dobson 2018

#include "target_memory_map.h"
#include "ee_printf.h"

// Target buffer
#define MAX_TARGET_MEMORY_SIZE 0x10000
unsigned char __pTargetBuffer[MAX_TARGET_MEMORY_SIZE];

// Target memory blocks
#define MAX_TARGET_MEMORY_BLOCKS 20

TargetMemoryBlock __targetMemoryBlocks[MAX_TARGET_MEMORY_BLOCKS];
int __targetMemoryBlockLastIdx = 0;

void targetClear()
{
    __targetMemoryBlockLastIdx = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_BLOCKS; i++)
        __targetMemoryBlocks[i].len = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_SIZE; i++)
        __pTargetBuffer[i] = 0;
}

void targetDataBlockStore(uint32_t addr, const uint8_t* pData, uint32_t len)
{
    // Check if contiguous with other data
    int blockIdx = -1;
    for (int i = 0; i < __targetMemoryBlockLastIdx; i++) {
        if (__targetMemoryBlocks[i].start + __targetMemoryBlocks[i].len == addr) {
            blockIdx = i;
            __targetMemoryBlocks[i].len += len;
            break;
        }
    }

    // New block
    if (blockIdx == -1) {
        if (__targetMemoryBlockLastIdx >= MAX_TARGET_MEMORY_BLOCKS) {
            ee_printf("Too many target memory blocks\n");
            return;
        }
        __targetMemoryBlocks[__targetMemoryBlockLastIdx].start = addr;
        __targetMemoryBlocks[__targetMemoryBlockLastIdx].len = len;
        __targetMemoryBlockLastIdx++;
    }

    // ee_printf("Blk st %04x len %d\n", addr, len);
    
    // Store block
    for (unsigned int i = 0; i < len; i++) {
        if (addr + i < MAX_TARGET_MEMORY_SIZE)
            __pTargetBuffer[addr + i] = pData[i];
    }
}

int targetGetNumBlocks()
{
    return __targetMemoryBlockLastIdx;
}

TargetMemoryBlock* targetGetMemoryBlock(int n)
{
    return &__targetMemoryBlocks[n];
}

unsigned char* targetMemoryPtr()
{
    return __pTargetBuffer;
}