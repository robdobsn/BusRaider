// Bus Raider
// Target Fonts
// Rob Dobson 2019

#include "TargetFonts.h"
#include <string.h>

TargetFonts::TargetFonts()
{
    _numFonts = 0;
}

TargetFonts::~TargetFonts()
{

}

void TargetFonts::addFont(TargetFont* pFontInfo)
{
    if (_numFonts < MAX_FONTS)
        _fontList[_numFonts++] = pFontInfo;
}

TargetFont* TargetFonts::getFont(const char* fontName)
{
    // Find font in list of fonts
    for (int i = 0; i < _numFonts; i++)
    {
        if (strcmp(_fontList[i]->_fontName, fontName) == 0)
            return _fontList[i];
    }
    return NULL;
}
