
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
  	bool proc([[maybe_unused]] FileParserDataCallback* pDataCallback, 
  				[[maybe_unused]] FileParserRegsCallback* pRegsCallback, 
				void* pCallbackParam,
  				[[maybe_unused]] const uint8_t* pData, [[maybe_unused]] int dataLen);
};
