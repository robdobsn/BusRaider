
#pragma once

#include "../System/logging.h"
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
	uint8_t extraData[100000];
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
		// LogWrite("Z80 Format", LOG_DEBUG, "Addr %02x %02x %02x %02x %02x %02x %02x %02x",
		// 			pDest[0], pDest[1], pDest[2], pDest[3], pDest[4], pDest[5], pDest[6], pDest[7]);
		return pOut-pDest;
	}

	// Info from https://www.worldofspectrum.org/faq/reference/z80format.htm
  	bool proc([[maybe_unused]] FileParserDataCallback* pDataCallback, 
  				[[maybe_unused]] FileParserRegsCallback* pRegsCallback, 
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

		// Calculate start of ram dump in extraData field
		int ramDumpOffset = 0;
		int extraHeaderLen = 0;
		if (versionNum > 1)
		{
			extraHeaderLen = (sna->extraData[0] + ((int)sna->extraData[1]) * 256);
			if (extraHeaderLen > 23)
				versionNum = 3;
			ramDumpOffset += extraHeaderLen;
		}

		// Convert
		static uint8_t ram[49152];
		int totalLenDecoded = 0;
		if (versionNum == 1)
		{
			// LogWrite(_logPrefix, LOG_DEBUG, "Version 1 Z80 file headerDataStartPos %d, compressed %c", ramDumpOffset, isCompressed ? 'Y' : 'N');
			totalLenDecoded = decompress(sna->extraData + ramDumpOffset, ram, 49152, isCompressed); 
			pDataCallback(16384, ram, 49152);
		}
		else
		{
			int curPos = extraHeaderLen + 2;
			while (true)
			{
				int compressedBlockLen = (sna->extraData[curPos]) + sna->extraData[curPos+1] * 256;
				int blockBase = -1;
				int blockPage = sna->extraData[curPos+2];
				// LogWrite(_logPrefix, LOG_DEBUG, "Decoding block curPos %d blockLen %d page %d", curPos, compressedBlockLen, blockPage);
				switch(blockPage)
				{
					case 4: blockBase = 0x8000; break;
					case 5: blockBase = 0xC000; break;
					case 8: blockBase = 0x4000; break;
				}
				if (blockBase != -1)
				{
					int lenDecoded = decompress(sna->extraData + curPos + 3, ram, 16384, true); 
					if (lenDecoded != 16384)
						LogWrite(_logPrefix, LOG_DEBUG, "Decode block len %d != 16384", lenDecoded);
					totalLenDecoded += lenDecoded;
					pDataCallback(blockBase, ram, 16384);
				}
				curPos += 3 + compressedBlockLen;
				if (30 + curPos >= dataLen)
					break;
			}
		}

		// Registers
		Z80Registers regs;
		regs.AF = (sna->a << 8) + (sna->f & 0xff);
		regs.AFDASH = (sna->a2 << 8) + (sna->f2 & 0xff);
		regs.BC = sna->bc;
		regs.BCDASH = sna->bc2;
		regs.DE = sna->de;
		regs.DEDASH = sna->de2;
		regs.HL = sna->hl;
		regs.HLDASH = sna->hl2;
		regs.IX = sna->ix;
		regs.IY = sna->iy;
		regs.SP = sna->sp;
		regs.PC = sna->pc;
		regs.I = sna->interrupt;
		regs.R = (sna->r & 0x7f) + ((sna->borderEtc & 0x01) >> 7);
		regs.INTMODE = sna->interruptMode & 0x03;
		regs.INTENABLED = sna->interruptFlipFlop;

		// Check for V2/3 PC
		if (versionNum > 1)
			regs.PC = (sna->extraData[2] + ((int)sna->extraData[3]) * 256);

		pRegsCallback(regs);

		// Done
		// LogWrite(_logPrefix, LOG_DEBUG, "OK format (v%d hdrLen %d compr %s) input len %d -> len %d run from %04x",
		// 			versionNum, extraHeaderLen, isCompressed ? "Y" : "N", dataLen, totalLenDecoded, regs.PC);
		// char regsStr[500];
		// regs.format(regsStr, 500);
		// LogWrite(_logPrefix, LOG_DEBUG, "Regs: %s", regsStr);
		return true;
	}
};
