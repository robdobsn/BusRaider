// Bus Raider
// Target Screen
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include "TargetFonts.h"
#include "TargetFont.h"
#include <circle/screen.h>
#include <circle/logger.h>

class TargetScreen
{
public:
    TargetScreen(CScreenDevice& screen, TargetFonts& targetFonts);
    ~TargetScreen();

    void setup(int tlx, int tly, int width, int height,
            const char* fontName,
            TScreenColor foregroundColour, TScreenColor backgroundColour,
            int borderWidth, int borderColour,
            int xPixScale = 1, int yPixScale = 1,
            int cellWidth = -1, int cellHeight = -1);

    void putChar(int ch, int xPos, int yPos);

private:
    CScreenDevice& _screen;
    TargetFonts& _targetFonts;
    TargetFont* _pFont;
    int _tlx;
    int _tly;
    int _width;
    int _height;
    int _cellWidth;
    int _cellHeight;
    int _xPixScale;
    int _yPixScale;
    TScreenColor _foregroundColour;
    TScreenColor _backgroundColour;
    int _borderWidth;
    int _borderColour;
};
