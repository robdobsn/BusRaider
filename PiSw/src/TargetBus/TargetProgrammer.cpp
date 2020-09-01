// Pi Bus Raider
// Rob Dobson 2018

#include "TargetProgrammer.h"
#include "../System/logging.h"

// Module name
static const char MODULE_PREFIX[] = "TargetProgrammer";

TargetProgrammer::TargetProgrammer()
{
    // Static vars
    _targetMemoryBlockLastIdx = 0;
    _targetRegsValid = false;
}

// Clear target
void TargetProgrammer::clear()
{
    _targetRegsValid = false;
    _targetMemoryBlockLastIdx = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_BLOCKS; i++)
        _targetMemoryBlocks[i].len = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_SIZE; i++)
        _pTargetMemoryImage[i] = 0;
}

void TargetProgrammer::addMemoryBlockStatic(uint32_t addr, const uint8_t* pData, uint32_t len, void* pProgrammer)
{
    if (pProgrammer)
        ((TargetProgrammer*)pProgrammer)->addMemoryBlock(addr, pData, len);
}

void TargetProgrammer::addMemoryBlock(uint32_t addr, const uint8_t* pData, uint32_t len)
{
    // Check if contiguous with other data
    int blockIdx = -1;
    for (int i = 0; i < _targetMemoryBlockLastIdx; i++) {
        if (_targetMemoryBlocks[i].start + _targetMemoryBlocks[i].len == addr) {
            blockIdx = i;
            _targetMemoryBlocks[i].len += len;
            break;
        }
    }

    // New block
    if (blockIdx == -1) 
    {
        if (_targetMemoryBlockLastIdx >= MAX_TARGET_MEMORY_BLOCKS) 
        {
            LogWrite(MODULE_PREFIX, LOG_DEBUG, "Too many target memory blocks\n");
            return;
        }
        _targetMemoryBlocks[_targetMemoryBlockLastIdx].start = addr;
        _targetMemoryBlocks[_targetMemoryBlockLastIdx].len = len;
        _targetMemoryBlockLastIdx++;
    }

    // ee_printf("Blk st %04x len %d\n", addr, len);
    
    // Store block
    for (unsigned int i = 0; i < len; i++) 
    {
        if (addr + i < MAX_TARGET_MEMORY_SIZE)
            _pTargetMemoryImage[addr + i] = pData[i];
    }
}

int TargetProgrammer::numMemoryBlocks()
{
    return _targetMemoryBlockLastIdx;
}

TargetProgrammer::TargetMemoryBlock* TargetProgrammer::getMemoryBlock(int n)
{
    return &_targetMemoryBlocks[n];
}

uint8_t* TargetProgrammer::getMemoryImagePtr()
{
    return _pTargetMemoryImage;
}

int TargetProgrammer::getMemoryImageSize()
{
    return MAX_TARGET_MEMORY_SIZE;
}

void TargetProgrammer::setTargetRegistersStatic(const Z80Registers& regs, void* pProgrammer)
{
    if (pProgrammer)
        ((TargetProgrammer*)pProgrammer)->setTargetRegisters(regs);
}

void TargetProgrammer::setTargetRegisters(const Z80Registers& regs)
{
    _targetRegisters = regs;
    _targetRegsValid = true;
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "SetTargetRegs PC=%04x SP=%04x", regs.PC, regs.SP);
}

bool TargetProgrammer::areRegistersValid()
{
    return _targetRegsValid;
}

void TargetProgrammer::getTargetRegs(Z80Registers& regs)
{
    regs = _targetRegisters;
    LogWrite(MODULE_PREFIX, LOG_VERBOSE, "getTargetRegs PC=%04x", regs.PC);
}

