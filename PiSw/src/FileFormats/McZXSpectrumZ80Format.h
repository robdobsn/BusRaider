
#pragma once

#include "../System/ee_printf.h"
#include <stdint.h>
#include <string.h>
#include "McFormatBase.h"

// https://www.worldofspectrum.org/faq/reference/z80format.htm
#pragma pack(push, 1)
typedef struct {
	uint8_t a, f;
	// byte order is LSB ie c is first...
	uint16_t bc, hl, pc, sp;
	uint8_t interrupt;
	uint8_t r;
	uint8_t borderEtc;
	uint16_t de;
	uint16_t bc2, de2, hl2;
	uint8_t a2, f2;
	uint16_t iy, ix;
	uint8_t interruptFlipFlop;
	uint8_t iff2;
	uint8_t interruptMode;
	uint8_t extraData[60000];
} version1Header_t;

#pragma pack(pop)

class McZXSpectrumZ80Format
{
  private:
    static const char* _logPrefix;

  public:
	McZXSpectrumZ80Format()
	{
	}

	int decompress(uint8_t* pIn, uint8_t* pDest, int maxLen, bool isCompressed)
	{
		uint8_t* pOut = pDest;
		while (pOut - pDest < maxLen)
		{
			if ((!isCompressed) || (*pIn != 0xED) || (*(pIn + 1) != 0xED))
			{
				*pOut++ = *pIn++;
			}
			else
			{
				// skip past the two eds
				pIn += 2;
				int length = *pIn++;
				if (length == 0)
					return pOut-pDest;
				int b = *pIn++;
				for (int i = 0; i < length; i++)
					*pOut++ = b;					
			}
		}
		return pOut-pDest;
	}

	// Info from https://www.worldofspectrum.org/faq/reference/z80format.htm
  	bool proc([[maybe_unused]] FileParserDataCallback* pDataCallback, 
  				[[maybe_unused]] FileParserAddrCallback* pAddrCallback, 
  				[[maybe_unused]] const uint8_t* pData, [[maybe_unused]] int dataLen)
	{
		// Has to be at least as long as the header plus some
		if (dataLen < 50)
		{
			LogWrite(_logPrefix, LOG_DEBUG, "Invalid format - not enough data\n");
			return false;
		}

		// Cast data for format template
		version1Header_t *sna = (version1Header_t *) pData;
		if (sna->borderEtc == 0xff)
			sna->borderEtc = 1;
		
		// Detect version
		int versionNum = 1;
		if (sna->pc == 0)
			versionNum = 2;

		// Check compressed
		bool isCompressed = true;
		if ((versionNum == 1) && ((sna->borderEtc & 0x20)) == 0)
			isCompressed = false;

		// Check for start of ram 
		int ramDumpOffset = 0;
		if (versionNum > 1)
		{
			int headerLen = (sna->extraData[30] + ((int)sna->extraData[31]) * 256);
			if (headerLen > 23)
				versionNum = 3;
			ramDumpOffset += headerLen;
		}

		// Convert
		uint8_t ram[49152];
		int lenDecoded = decompress(sna->extraData + ramDumpOffset, ram, 49152, isCompressed); 
		pDataCallback(16384, ram, 49152); 
		LogWrite(_logPrefix, LOG_DEBUG, "OK format (v%d, compression %s) input len %d -> len %d\n",
					versionNum, isCompressed ? "Y" : "N", dataLen, lenDecoded);
		return true;
	}
};
