
#pragma once

#include <stdint.h>
#include <string.h>
#include "McFormatBase.h"

class McZXSpectrumZ80Format
{
  public:
	McZXSpectrumZ80Format();
	int decompress(uint8_t* pIn, uint8_t* pDest, int maxLen, bool isCompressed);

	// Info from https://www.worldofspectrum.org/faq/reference/z80format.htm
  	bool proc( FileParserDataCallback* pDataCallback, 
  				 FileParserRegsCallback* pRegsCallback, 
				void* pCallbackParam,
  				 const uint8_t* pData,  int dataLen);
};
