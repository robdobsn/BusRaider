// Pi Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TargetMemoryBlock {
    uint32_t start;
    uint32_t len;
} TargetMemoryBlock;

extern void targetClear();
extern void targetDataBlockStore(uint32_t addr, const uint8_t* pData, uint32_t len);
extern int targetGetNumBlocks();
extern TargetMemoryBlock* targetGetMemoryBlock(int n);
extern unsigned char* targetMemoryPtr();

#ifdef __cplusplus
}
#endif
