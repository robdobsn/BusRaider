// Pi Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include "../TargetBus/TargetRegisters.h"
#include "../TargetBus/TargetCPU.h"

class TargetState
{
public:
    // Max 1MB address range of Z180 so use that as the limit of memory
    static const int MAX_TARGET_MEMORY_SIZE = 1024 * 1024;
    static const int MAX_TARGET_MEMORY_BLOCKS = 20;

    typedef struct TargetMemoryBlock {
        uint32_t start;
        uint32_t len;
    } TargetMemoryBlock;

    // Target memory image
    static uint8_t _pTargetMemoryImage[MAX_TARGET_MEMORY_SIZE];

    // Target memory blocks
    static TargetMemoryBlock _targetMemoryBlocks[MAX_TARGET_MEMORY_BLOCKS];
    static int _targetMemoryBlockLastIdx;

    // Registers
    static bool _targetRegsValid;
    static Z80Registers _targetRegisters;

public:
    static void clear();
    static void addMemoryBlock(uint32_t addr, const uint8_t* pData, uint32_t len);
    static int numMemoryBlocks();
    static TargetMemoryBlock* getMemoryBlock(int n);
    static unsigned char* getMemoryImagePtr();
    static int getMemoryImageSize();
    static void setTargetRegisters(const Z80Registers& regs);
    static bool areRegistersValid();
    static void getTargetRegs(Z80Registers& regs);
};
