// Bus Raider
// Target Fonts
// Rob Dobson 2019

#pragma once

#include "TargetFont.h"

class TargetFonts
{
public:
    TargetFonts();
    ~TargetFonts();
    void addFont(TargetFont* pFontInfo);
    TargetFont* getFont(const char* fontName);

public:
    static const int MAX_FONTS = 20;
    TargetFont* _fontList[MAX_FONTS];
    int _numFonts;
};
