// Pi Bus Raider
// Rob Dobson 2018

#include "TargetImager.h"
#include "logging.h"

// Module name
static const char MODULE_PREFIX[] = "TargetImager";

TargetImager::TargetImager()
{
    // Static vars
    _targetMemoryBlockLastIdx = 0;
    _targetRegsValid = false;
}

// Clear target
void TargetImager::clear()
{
    _targetRegsValid = false;
    _targetMemoryBlockLastIdx = 0;

    for (uint32_t i = 0; i < MAX_TARGET_MEMORY_BLOCKS; i++)
        _targetMemoryBlocks[i].len = 0;

    for (uint32_t i = 0; i < MAX_TARGET_MEMORY_SIZE; i++)
        _pTargetMemoryImage[i] = 0;
}

void TargetImager::addMemoryBlockStatic(uint32_t addr, const uint8_t* pData, uint32_t len, void* pProgrammer)
{
    if (pProgrammer)
        ((TargetImager*)pProgrammer)->addMemoryBlock(addr, pData, len);
}

void TargetImager::addMemoryBlock(uint32_t addr, const uint8_t* pData, uint32_t len)
{
    // Check if contiguous with other data
    int blockIdx = -1;
    for (uint32_t i = 0; i < _targetMemoryBlockLastIdx; i++) {
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
    for (uint32_t i = 0; i < len; i++) 
    {
        if (addr + i < MAX_TARGET_MEMORY_SIZE)
            _pTargetMemoryImage[addr + i] = pData[i];
    }
}

uint32_t TargetImager::numMemoryBlocks()
{
    return _targetMemoryBlockLastIdx;
}

TargetImager::TargetMemoryBlock* TargetImager::getMemoryBlock(uint32_t n)
{
    return &_targetMemoryBlocks[n];
}

uint8_t* TargetImager::getMemoryImagePtr()
{
    return _pTargetMemoryImage;
}

uint32_t TargetImager::getMemoryImageSize()
{
    return MAX_TARGET_MEMORY_SIZE;
}

void TargetImager::setTargetRegistersStatic(const Z80Registers& regs, void* pProgrammer)
{
    if (pProgrammer)
        ((TargetImager*)pProgrammer)->setTargetRegisters(regs);
}

void TargetImager::setTargetRegisters(const Z80Registers& regs)
{
    _targetRegisters = regs;
    _targetRegsValid = true;
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "SetTargetRegs PC=%04x SP=%04x", regs.PC, regs.SP);
}

bool TargetImager::areRegistersValid()
{
    return _targetRegsValid;
}

void TargetImager::getTargetRegs(Z80Registers& regs)
{
    regs = _targetRegisters;
    LogWrite(MODULE_PREFIX, LOG_VERBOSE, "getTargetRegs PC=%04x", regs.PC);
}

