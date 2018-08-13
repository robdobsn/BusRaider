// Pi Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"

typedef struct TargetMemoryBlock {
    uint32_t start;
    uint32_t len;
} TargetMemoryBlock;

extern void targetClear();
extern void targetDataBlockCallback(uint32_t addr, const uint8_t* pData, uint32_t len);
extern int targetGetNumBlocks();
extern TargetMemoryBlock* targetGetMemoryBlock(int n);
extern unsigned char* targetMemoryPtr();