// Bus Raider
// Rob Dobson 2019

#pragma once

#include "stddef.h"
#include "stdint.h"
#include "wgfxfont.h"

enum DISPLAY_FX_COLOUR
{
    DISPLAY_FX_DEFAULT = -1,
	DISPLAY_FX_BLACK = 0,
	DISPLAY_FX_DARK_RED,
	DISPLAY_FX_DARK_GREEN,
	DISPLAY_FX_DARK_YELLOW,
	DISPLAY_FX_DARK_BLUE,
	DISPLAY_FX_DARK_PURPLE,
	DISPLAY_FX_DARK_CYAN,
	DISPLAY_FX_GRAY,
	DISPLAY_FX_DARK_GRAY,
	DISPLAY_FX_RED,
	DISPLAY_FX_GREEN,
	DISPLAY_FX_YELLOW,
	DISPLAY_FX_BLUE,
	DISPLAY_FX_PURPLE,
	DISPLAY_FX_CYAN,
	DISPLAY_FX_WHITE
};

class FrameBufferInfo
{
public:
    uint8_t* pFB;
    int pixelsWidth;
    int pixelsHeight;
    int pitch;
    uint8_t* pFBWindow;
    int pixelsWidthWindow;
    int pixelsHeightWindow;
    int bytesPerPixel;
};

class DisplayBase
{
public:
    virtual void foreground(DISPLAY_FX_COLOUR colour) = 0;
    virtual void background(DISPLAY_FX_COLOUR colour) = 0;
    virtual void write(int col, int row, const char* pStr) = 0;
    virtual void write(int col, int row, int ch) = 0;
    virtual void setPixel(int x, int y, int value, DISPLAY_FX_COLOUR colour) = 0;
    virtual void getFramebuffer(FrameBufferInfo& frameBufferInfo) = 0;
    // Target
    virtual void targetLayout(
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour);

};
