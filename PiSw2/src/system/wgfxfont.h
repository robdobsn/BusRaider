// BusRaider
// Rob Dobson 2018-2019

#pragma once

#include <stdint.h>

typedef struct WgfxFont {
    uint32_t cellX;
    uint32_t cellY;
    uint32_t bytesAcross;
    uint32_t bytesPerChar;
    unsigned char* pFontData;
    uint32_t fontNumChars;
} WgfxFont;

