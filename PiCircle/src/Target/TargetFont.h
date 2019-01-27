// Bus Raider
// Target Font
// Rob Dobson 2019

#pragma once

#include "circle/string.h"
#include <circle/logger.h>
#include <stdlib.h>
#include <stdint.h>

class TargetFont
{
public:
    TargetFont()
    {
        _pFontData = NULL;
    }
    TargetFont(const char* fontName, int cellX, int cellY, int bytesAcross, int bytesPerChar, 
            int firstChar, int lastChar, int charCount, unsigned char* pFontData)
    {
        _fontName = fontName;
        _cellX = cellX;
        _cellY = cellY;
        _bytesAcross = bytesAcross;
        _bytesPerChar = bytesPerChar;
        _firstChar = firstChar;
        _lastChar = lastChar;
        _charCount = charCount;
        _pFontData = pFontData;
    }
    int getCellWidth()
    {
        return _cellX;
    }
    int getCellHeight()
    {
        return _cellY;
    }
    bool getPixel(int chAscii, int nPosX, int nPosY) const
    {
	    uint8_t nAscii = (uint8_t) chAscii;
        if (nAscii < _firstChar || nAscii > _lastChar)
            return false;
        int nIndex = nAscii - _firstChar;
        assert (nIndex < _charCount);
        assert (nPosX < _cellX);
        if (nPosY >= _cellY)
            return false;
        return (_pFontData[nIndex * _bytesPerChar + (nPosX/8) * _bytesAcross + nPosY * _bytesAcross] & (0x80 >> ((nPosX % 8) + (nPosX < 8 ? (8 - _cellX % 8) : 0) ))) != 0;
    }

public:
    const char* _fontName;
    int _cellX;
    int _cellY;
    int _bytesAcross;
    int _bytesPerChar;
    int _firstChar;
    int _lastChar;
    int _charCount;
    unsigned char* _pFontData;
};
