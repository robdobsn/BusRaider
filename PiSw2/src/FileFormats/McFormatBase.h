// Format base
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include "CPUHandler_Z80Regs.h"

typedef void FileParserDataCallback(uint32_t addr, const uint8_t* pData, uint32_t len, void* pParam);
typedef void FileParserRegsCallback(const Z80Registers& regs, void* pParam);
