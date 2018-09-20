// Bus Raider
// Rob Dobson 2018

#pragma once

#include "globaldefs.h"
#include "McFormatBase.h"

class McTRS80CmdFormat
{
  private:
    static const char* _logPrefix;

  public:
  	McTRS80CmdFormat();
  	void proc(FileParserDataCallback* pDataCallback,
  			FileParserAddrCallback* pAddrCallback,
  			const uint8_t* pData, int dataLen);
};
