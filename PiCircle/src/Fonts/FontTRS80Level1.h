// Bus Raider
// |Font
// Rob Dobson 2019

#pragma once

#include "../Target/TargetFont.h"

class FontTRS80Level1
{
public:
    static TargetFont _font;
    static TargetFont* getFont() { return &_font; };
};

