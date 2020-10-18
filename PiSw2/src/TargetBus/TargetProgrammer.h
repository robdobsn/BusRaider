// Pi Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include "../TargetBus/TargetRegisters.h"
#include "../TargetBus/TargetCPU.h"

class TargetProgrammer
{
public:
    TargetProgrammer();

    void init()
    {
    }

    void service()
    {
    }

    void clear();

    // Max 1MB address range of Z180 so use that as the limit of memory
    static const uint32_t MAX_TARGET_MEMORY_SIZE = 1024 * 1024;
    static const uint32_t MAX_TARGET_MEMORY_BLOCKS = 20;

    typedef struct TargetMemoryBlock {
        uint32_t start;
        uint32_t len;
    } TargetMemoryBlock;

    // Target memory image
    uint8_t _pTargetMemoryImage[MAX_TARGET_MEMORY_SIZE];

    // Target memory blocks
    TargetMemoryBlock _targetMemoryBlocks[MAX_TARGET_MEMORY_BLOCKS];
    uint32_t _targetMemoryBlockLastIdx;

    // Registers
    bool _targetRegsValid;
    Z80Registers _targetRegisters;

public:
    static void addMemoryBlockStatic(uint32_t addr, const uint8_t* pData, uint32_t len, void* pProgrammer);
    void addMemoryBlock(uint32_t addr, const uint8_t* pData, uint32_t len);
    uint32_t numMemoryBlocks();
    TargetMemoryBlock* getMemoryBlock(uint32_t n);
    unsigned char* getMemoryImagePtr();
    uint32_t getMemoryImageSize();
    static void setTargetRegistersStatic(const Z80Registers& regs, void* pProgrammer);
    void setTargetRegisters(const Z80Registers& regs);
    bool areRegistersValid();
    void getTargetRegs(Z80Registers& regs);
};
