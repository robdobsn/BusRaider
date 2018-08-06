// Pi Bus Raider
// Rob Dobson 2018

#pragma once

typedef struct WgfxFont {
    int cellX;
    int cellY;
    int bytesAcross;
    int bytesPerChar;
    unsigned char* pFontData;
} WgfxFont;