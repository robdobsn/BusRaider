// Bus Raider
// Rob Dobson 2019

#pragma once

#include "stddef.h"
#include "stdint.h"
#include "../system/wgfxfont.h"
#include <circle/device.h>

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

class DisplayBase : public CDevice
{
public:
    virtual ~DisplayBase()
    {
    }
    // TODO 2020 remove
    virtual void debug()
    {}

    virtual void foreground(DISPLAY_FX_COLOUR colour)
    {
    }
    virtual void background(DISPLAY_FX_COLOUR colour)
    {
    }
    virtual void write(uint32_t col, uint32_t row, const uint8_t* pStr)
    {
    }
    virtual void write(uint32_t col, uint32_t row, uint32_t ch)
    {
    }
    virtual void setPixel(uint32_t x, uint32_t y, uint32_t value, DISPLAY_FX_COLOUR colour)
    {
    }
    virtual void getFrameBufferInfo(FrameBufferInfo& frameBufferInfo)
    {
    }
    
    // Target
    virtual void targetLayout(
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour)
    {
    }

};
