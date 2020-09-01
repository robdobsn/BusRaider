
#pragma once

#include <stdint.h>
#include <string.h>
#include "McFormatBase.h"

class McZXSpectrumSNAFormat
{
  public:
	McZXSpectrumSNAFormat();

	// Info from https://www.worldofspectrum.org/faq/reference/formats.htm
  	bool proc([[maybe_unused]]FileParserDataCallback* pDataCallback, 
  				[[maybe_unused]] FileParserRegsCallback* pRegsCallback, 
				void* pCallbackParam,
  				[[maybe_unused]] const uint8_t* pData, [[maybe_unused]] int dataLen);
};
