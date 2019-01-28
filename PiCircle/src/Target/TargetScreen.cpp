// Bus Raider
// Target Screen
// Rob Dobson 2019

#include "TargetScreen.h"

TargetScreen::TargetScreen(CScreenDevice& screen, TargetFonts& targetFonts) :
             _screen(screen), _targetFonts(targetFonts)
{
    _pFont = NULL;
}

TargetScreen::~TargetScreen()
{

}

void TargetScreen::setup(int tlx, int tly, int width, int height,
        const char* fontName, 
        TScreenColor foregroundColour, TScreenColor backgroundColour,
        int borderWidth, int borderColour,
        int xPixScale, int yPixScale,
        int cellWidth, int cellHeight)
{
    // Find the font
    _pFont = _targetFonts.getFont(fontName);
    assert(_pFont != NULL);
    _tlx = tlx;
    _tly = tly;
    _width = width;
    _height = height;
    _cellWidth = cellWidth;
    if ((cellWidth == -1) && (_pFont))
        _cellWidth = _pFont->getCellWidth();
    _cellHeight = cellHeight;
    if ((cellHeight == -1) && (_pFont))
        _cellHeight = _pFont->getCellHeight();
    _xPixScale = xPixScale;
    _yPixScale = yPixScale;
    _foregroundColour = foregroundColour;
    _backgroundColour = backgroundColour;
    _borderWidth = borderWidth;
    _borderColour = borderColour;
}

void TargetScreen::putChar(int ch, int xPos, int yPos)
{
    if (!_pFont)
        return;

    // Show character
    int nPosX = xPos * _cellWidth * _xPixScale;
    int nPosY = yPos * _cellHeight * _yPixScale;
    // CLogger::Get()->Write("TargetScreen", LogNotice, "xPos %d yPos %d cellHeight %d yScale %d cellWidth %d xScale %d foreG %d backG %d tColour %d",
    //             nPosX, nPosY, _cellHeight, _yPixScale, _cellWidth, _xPixScale, _foregroundColour, _backgroundColour, sizeof(TScreenColor));
    for (int y = 0; y < _cellHeight; y++) 
    {
        for (int x = 0; x < _cellWidth; x++) 
        {
            TScreenColor pixColour = _pFont->getPixel(ch, x, y) ? _foregroundColour : _backgroundColour;
            for (int i = 0; i < _yPixScale; i++) 
            {
                for (int j = 0; j < _xPixScale; j++) 
                {
                    _screen.SetPixel(nPosX + x * _xPixScale + j, nPosY + y * _yPixScale + i, pixColour);
                }
            }
        }
    }
}
