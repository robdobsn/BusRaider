
#pragma once

#include "logging.h"
#include <stdint.h>
#include <string.h>
#include "McFormatBase.h"

class McZXSpectrumTZXFormat
{
  public:
	McZXSpectrumTZXFormat();

	// Info from http://www.zx-modules.de/fileformats/tapformat.html
	bool decodeTapBlock(FileParserDataCallback* pDataCallback,
		 FileParserRegsCallback* pRegsCallback,
		void* pCallbackParam,
		const uint8_t* pData, int curOffset, int totalLen, int dataLen);
  	bool proc(FileParserDataCallback* pDataCallback, 
  				 FileParserRegsCallback* pRegsCallback, 
				void* pCallbackParam,
  				const uint8_t* pData, int dataLen);

private:
  	void getString(const uint8_t* pData, int dataLen,
  				int recPos, int strLen, char* pDest);
  	uint32_t getWord16(const uint8_t* pData, int dataLen, int recPos);
	uint32_t getByte(const uint8_t* pData, int dataLen, int recPos);

  	static const int MAX_DESCR_LEN = 256;
	int _curHeaderType;
	int _curDataLen;
	int _curAutostartLine;
	int _curProgramLen;
	int _curVarName;
	int _curStartAddr;
};