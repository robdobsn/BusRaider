
#pragma once

#include <stdint.h>
#include <string.h>
#include "McFormatBase.h"

class McZXSpectrumSNAFormat
{
  public:
	McZXSpectrumSNAFormat();

	// Info from https://www.worldofspectrum.org/faq/reference/formats.htm
  	bool proc(FileParserDataCallback* pDataCallback, 
  				 FileParserRegsCallback* pRegsCallback, 
				void* pCallbackParam,
  				 const uint8_t* pData,  int dataLen);
};
