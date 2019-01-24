// Format base
// Rob Dobson 2018

#pragma once

#include <stdint.h>

typedef void FileParserDataCallback(uint32_t addr, const uint8_t* pData, uint32_t len);
typedef void FileParserAddrCallback(uint32_t execAddr);
