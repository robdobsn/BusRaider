// Bus Raider
// Rob Dobson 2019

#pragma once

#include "wgfxfont.h"
#include "stdint.h"

class Display
{
public:
    Display();
    ~Display();

    bool init(int displayWidth, int displayHeight);

    void targetLayout(int tlX, int tlY,
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour, 
                    int borderWidth, int borderColour);

    void targetSetChar();

    void windowWrite(int winIdx, int col, int row, const uint8_t* pStr);

    void termWrite(const char* pStr);
    void termWrite(int ch);

    void termColour(int colour);
    int termGetWidth();

private:
    bool _displayStarted;

};