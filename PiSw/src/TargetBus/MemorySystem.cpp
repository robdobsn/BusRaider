// Bus Raider
// Rob Dobson 2019

#include "MemorySystem.h"
#include <string.h>
#include <stdlib.h>
#include "../System/ee_printf.h"
#include "../System/lowlib.h"
#include "../System/rdutils.h"

// Module name
static const char FromMemorySystem[] = "MemorySystem";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MemorySystem::MemorySystem(uint32_t memorySize)
{
    _pTargetMemBuffer = NULL;
    _targetMemSize = memorySize;
}

MemorySystem::~MemorySystem()
{
    if (!_pTargetMemBuffer)
        return;
    delete [] _pTargetMemBuffer;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t* MemorySystem::checkAlloc()
{
    if (!_pTargetMemBuffer)
        _pTargetMemBuffer = new uint8_t[_targetMemSize];
    return _pTargetMemBuffer;
}

uint8_t MemorySystem::getMemoryByte(uint32_t addr)
{
    if ((addr >= _targetMemSize) || (!checkAlloc()))
        return 0;
    return _pTargetMemBuffer[addr];
}

void MemorySystem::putMemoryByte(uint32_t addr, uint32_t data)
{
    if ((addr >= _targetMemSize) || (!checkAlloc()))
        return;
    _pTargetMemBuffer[addr] = data;
}


uint16_t MemorySystem::getMemoryWord(uint32_t addr)
{
    if ((addr >= _targetMemSize) || (!checkAlloc()))
        return 0;
    return _pTargetMemBuffer[(addr+1) % _targetMemSize] * 256 + _pTargetMemBuffer[addr];
}

void MemorySystem::clearMemory()
{
    if (!checkAlloc())
        return;
    memset(_pTargetMemBuffer, 0, _targetMemSize);
}

bool MemorySystem::blockWrite(uint32_t addr, const uint8_t* pBuf, uint32_t len)
{
    int copyLen = len;
    if ((addr >= _targetMemSize) || (!checkAlloc()))
    {
        LogWrite(FromMemorySystem, LOG_DEBUG, "blockWrite failed to %04x len %d ptr %08x", addr, len, _pTargetMemBuffer);
        return false;
    }
    if (addr + copyLen > _targetMemSize)
        copyLen = _targetMemSize - addr;
    memcpy(_pTargetMemBuffer + addr, pBuf, copyLen);
    return true;
}

bool MemorySystem::blockRead(uint32_t addr, uint8_t* pBuf, uint32_t len)
{
    // if ((!_pTargetMachine) || (!_pTargetMachine->getDescriptorTable()->emulatedRAM))
    // {
    //     LogWrite(FromTargetDebug, LOG_DEBUG, "blockRead: Not in single step or emulated RAM %04x %d", addr, len);
    //     return false;
    // }
    int copyLen = len;
    if ((addr >= _targetMemSize) || (!checkAlloc()))
    {
        LogWrite(FromMemorySystem, LOG_DEBUG, "blockRead: Too long %04x %d", addr, len);
        return false;
    }
    if (addr + copyLen > _targetMemSize)
        copyLen = _targetMemSize - addr;
    memcpy(pBuf, _pTargetMemBuffer + addr, copyLen);
    return true;
}

uint8_t* MemorySystem::getMemBuffer()
{
    return checkAlloc();
}

int MemorySystem::getMemBufferLen()
{
    return _targetMemSize;
}

