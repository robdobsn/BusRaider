// Bus Raider
// Rob Dobson 2019

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "../Machines/McBase.h"
#include "../TargetBus/TargetCPU.h"

class RAMEmulator
{
public:
    // Setup
    static void setup(McBase* pTargetMachine);

    // Service
    static void service();

    // Interrupt handler
    static uint32_t handleInterrupt([[maybe_unused]] uint32_t addr, [[maybe_unused]] uint32_t data, 
            [[maybe_unused]] uint32_t flags, [[maybe_unused]] uint32_t retVal,
            [[maybe_unused]] McDescriptorTable& descriptorTable);

    // Memory access
    static uint8_t getMemoryByte(uint32_t addr);
    static uint16_t getMemoryWord(uint32_t addr);
    static void clearMemory();
    static bool blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len);
    static bool blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len);

    // Direct access
    static uint8_t* getMemBuffer();
    static int getMemBufferLen();

private:

    // Machine being debugged
    static McBase* _pTargetMachine;

    // Paging of RAM/ROM during read from emulated RAM
    static bool _emulatedRAMReadPagingActive;

    // Target memory buffer  
    static const int MAX_TARGET_MEM_ADDR = 0xffff;
    static uint8_t _targetMemBuffer[STD_TARGET_MEMORY_LEN];

};
