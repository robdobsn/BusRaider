// Pi Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include "CPUHandler_Z80Regs.h"
#include "BusAccess.h"

class TargetImager
{
public:
    TargetImager();

    typedef struct TargetMemoryBlock {
        uint32_t start;
        uint32_t len;
    } TargetMemoryBlock;

    void init()
    {
    }

    void service()
    {
    }

    void clear();

    // Set location in memory to use for temporary register setup during snapshot
    // and program execution - this is often part of the screen memory to avoid important
    // program memory
    void setSetRegistersCodeAddr(uint32_t setRegistersCodeAddr)
    {
        _setRegistersCodeAddr = setRegistersCodeAddr;
    }
    uint32_t getSetRegistersCodeAddr()
    {
        return _setRegistersCodeAddr;
    }

    static void addMemoryBlockStatic(uint32_t addr, const uint8_t* pData, uint32_t len, void* pProgrammer);
    void addMemoryBlock(uint32_t addr, const uint8_t* pData, uint32_t len);
    uint32_t numMemoryBlocks();
    TargetMemoryBlock* getMemoryBlock(uint32_t n);
    unsigned char* getMemoryImagePtr();
    uint32_t getMemoryImageSize();
    static void setCPURegistersStatic(const Z80Registers& regs, void* pProgrammer);
    void setCPURegisters(const Z80Registers& regs);
    bool areRegistersValid();
    void getTargetRegs(Z80Registers& regs);

private:
    // Max 1MB address range of Z180 so use that as the limit of memory
    static const uint32_t MAX_TARGET_MEMORY_SIZE = 1024 * 1024;
    static const uint32_t MAX_TARGET_MEMORY_BLOCKS = 100;

    // Target memory image
    uint8_t _pTargetMemoryImage[MAX_TARGET_MEMORY_SIZE];

    // Target memory blocks
    TargetMemoryBlock _targetMemoryBlocks[MAX_TARGET_MEMORY_BLOCKS];
    uint32_t _targetMemoryBlockLastIdx;

    // Registers
    bool _targetRegsValid;
    Z80Registers _cpuRegisters;

    // Set registers code addr
    uint32_t _setRegistersCodeAddr;
};
