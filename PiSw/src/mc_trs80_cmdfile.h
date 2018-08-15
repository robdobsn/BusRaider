// Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"

typedef void CmdFileParserDataCallback(uint32_t addr, const uint8_t* pData, uint32_t len);
typedef void CmdFileParserAddrCallback(uint32_t execAddr);

extern void mc_trs80_cmdfile_init();

extern void mc_trs80_cmdfile_proc(CmdFileParserDataCallback* pDataCallback, CmdFileParserAddrCallback* pAddrCallback, const uint8_t* pData, int dataLen);
