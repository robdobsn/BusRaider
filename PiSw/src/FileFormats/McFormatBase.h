// Format base
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include "../TargetBus/TargetRegisters.h"

typedef void FileParserDataCallback(uint32_t addr, const uint8_t* pData, uint32_t len);
typedef void FileParserRegsCallback(const Z80Registers& regs);
