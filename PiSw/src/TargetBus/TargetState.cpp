// Pi Bus Raider
// Rob Dobson 2018

#include "TargetState.h"
#include "../System/ee_printf.h"

// Module name
static const char FromTargetState[] = "TargetState";

// Static vars
uint8_t TargetState::_pTargetMemoryImage[TargetState::MAX_TARGET_MEMORY_SIZE];
TargetState::TargetMemoryBlock TargetState::_targetMemoryBlocks[MAX_TARGET_MEMORY_BLOCKS];
int TargetState::_targetMemoryBlockLastIdx = 0;
bool TargetState::_targetRegsValid = false;
Z80Registers TargetState::_targetRegisters;

// Clear target
void TargetState::clear()
{
    _targetMemoryBlockLastIdx = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_BLOCKS; i++)
        _targetMemoryBlocks[i].len = 0;

    for (int i = 0; i < MAX_TARGET_MEMORY_SIZE; i++)
        _pTargetMemoryImage[i] = 0;
}

void TargetState::addMemoryBlock(uint32_t addr, const uint8_t* pData, uint32_t len)
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
            LogWrite(FromTargetState, LOG_DEBUG, "Too many target memory blocks\n");
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

int TargetState::numMemoryBlocks()
{
    return _targetMemoryBlockLastIdx;
}

TargetState::TargetMemoryBlock* TargetState::getMemoryBlock(int n)
{
    return &_targetMemoryBlocks[n];
}

uint8_t* TargetState::getMemoryImagePtr()
{
    return _pTargetMemoryImage;
}

void TargetState::setTargetRegisters(Z80Registers& regs)
{
    _targetRegisters = regs;
    _targetRegsValid = true;
    LogWrite(FromTargetState, LOG_DEBUG, "SetTargetRegs PC=%04x\n", regs.PC);
}

bool TargetState::areRegistersValid()
{
    return _targetRegsValid;
}

void TargetState::getTargetRegsAndInvalidate(Z80Registers& regs)
{
    regs = _targetRegisters;
    _targetRegsValid = false;
    LogWrite(FromTargetState, LOG_DEBUG, "getTargetRegsAndInvalidate PC=%04x\n", regs.PC);
}

