// Bus Raider
// Rob Dobson 2018

#pragma once

#include "McFormatBase.h"

class McTRS80CmdFormat
{
  private:
    static const char* _logPrefix;

  public:
  	McTRS80CmdFormat();
  	void proc(FileParserDataCallback* pDataCallback,
  			FileParserRegsCallback* pRegsCallback,
  			const uint8_t* pData, int dataLen);
};
