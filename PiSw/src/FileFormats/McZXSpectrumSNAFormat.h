
#pragma once

#include "../System/ee_printf.h"
#include <stdint.h>
#include <string.h>
#include "McFormatBase.h"

// https://www.worldofspectrum.org/faq/reference/formats.htm
#pragma pack(push, 1)
typedef struct {
	uint8_t i;
	uint16_t hl2, de2, bc2, af2;
	uint16_t hl, de, bc;
	uint8_t interrupt;
	uint8_t r;
	uint16_t af, sp;
	uint8_t intMode;
	uint8_t borderColor;
	uint8_t ramDump[49152]; // RAM dump 16384 to 65535
} sna_t;
#pragma pack(pop)

class McZXSpectrumSNAFormat
{
  private:
    static const char* _logPrefix;

  public:
	McZXSpectrumSNAFormat()
	{
	}

		// Info from https://www.worldofspectrum.org/faq/reference/formats.htm
  	bool proc([[maybe_unused]]FileParserDataCallback* pDataCallback, 
  				[[maybe_unused]] FileParserRegsCallback* pRegsCallback, 
  				[[maybe_unused]] const uint8_t* pData, [[maybe_unused]] int dataLen)
	{
		if (dataLen < (int) sizeof(sna_t))
		{
			LogWrite(_logPrefix, LOG_DEBUG, "Invalid format - not enough data\n");
			return false;
		}
		sna_t *sna = (sna_t *) pData;
		pDataCallback(16384, sna->ramDump, 49152); 
		LogWrite(_logPrefix, LOG_DEBUG, "OK format\n");
		return true;
	}
};