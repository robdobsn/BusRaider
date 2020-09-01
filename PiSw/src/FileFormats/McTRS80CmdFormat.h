// Bus Raider
// Rob Dobson 2018

#pragma once

#include "McFormatBase.h"

class McTRS80CmdFormat
{
  public:
  	McTRS80CmdFormat();
  	void proc(FileParserDataCallback* pDataCallback,
  			FileParserRegsCallback* pRegsCallback,
			void* pCallbackParam,
  			const uint8_t* pData, int dataLen);
};
