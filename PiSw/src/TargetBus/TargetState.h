// Pi Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include "../TargetBus/TargetRegisters.h"

class TargetState
{
public:
    // Consts
    static const int MAX_TARGET_MEMORY_SIZE = 0x10000;
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
    static void setTargetRegisters(Z80Registers& regs);
    static bool areRegistersValid();
    static void getTargetRegsAndInvalidate(Z80Registers& regs);

};
