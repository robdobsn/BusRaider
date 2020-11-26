// BusRaider
// Rob Dobson 2018-2019

#pragma once

#include <stdint.h>

#define MAX_FONT_NAME_LEN 30
typedef struct WgfxFont {
    uint32_t cellX;
    uint32_t cellY;
    uint32_t bytesAcross;
    uint32_t bytesPerChar;
    unsigned char* pFontData;
    uint32_t fontNumChars;
    char fontName[MAX_FONT_NAME_LEN];
} WgfxFont;

