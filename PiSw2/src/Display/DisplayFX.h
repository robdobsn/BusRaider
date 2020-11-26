// Bus Raider
// Rob Dobson 2019

#pragma once

#include "DisplayBase.h"
#include "../system/wgfxfont.h"
#include "stdint.h"
#include "stddef.h"
#include <circle/bcmframebuffer.h>
#include <circle/types.h>

extern "C" WgfxFont __systemFont;

#define DEPTH	8		// can be: 8, 16 or 32

#define COLORTRANS(RGB) (((RGB & 0xff) << 16) \
                    | ((RGB & 0xff00)) \
                    | ((RGB & 0xff0000) >> 16))

// really ((green) & 0x3F) << 5, but to have a 0-31 range for all colors
#define COLOR16(red, green, blue)	  (((red) & 0x1F) << 11 \
					| ((green) & 0x1F) << 6 \
					| ((blue) & 0x1F))

// BGRA (was RGBA with older firmware)
#define COLOR32(red, green, blue, alpha)  (((blue) & 0xFF)       \
					| ((green) & 0xFF) << 8  \
					| ((red) & 0xFF)   << 16 \
					| ((alpha) & 0xFF) << 24)

#define BLACK_COLOR	0

#if DEPTH == 8
	typedef u8 TScreenColor;

	#define NORMAL_COLOR16			COLOR16 (31, 31, 31)
	#define HIGH_COLOR16			COLOR16 (31, 0, 0)
	#define HALF_COLOR16			COLOR16 (0, 0, 31)

	#define NORMAL_COLOR			1
	#define HIGH_COLOR			2
	#define HALF_COLOR			3
#elif DEPTH == 16
	typedef u16 TScreenColor;

	#define NORMAL_COLOR			COLOR16 (31, 31, 31)
	#define HIGH_COLOR			COLOR16 (31, 0, 0)
	#define HALF_COLOR			COLOR16 (0, 0, 31)
#elif DEPTH == 32
	typedef u32 TScreenColor;

	#define NORMAL_COLOR			COLOR32 (255, 255, 255, 255)
	#define HIGH_COLOR			COLOR32 (255, 0, 0, 255)
	#define HALF_COLOR			COLOR32 (0, 0, 255, 255)
#else
	#error DEPTH must be 8, 16 or 32
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Window
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class WindowContent
{
public:
    WindowContent()
    {
        _numLines = 0;
        _numCharsAcross = 0;
        _pChars = NULL;
    }
    ~WindowContent()
    {
        if (_pChars)
            delete [] _pChars;
    }
    void setSize(uint32_t numLines, uint32_t numCharsAcross)
    {
        if (_pChars)
            delete [] _pChars;
        _pChars = NULL;
        _numLines = numLines;
        _numCharsAcross = numCharsAcross;
        _pChars = new uint8_t[_numLines * _numCharsAcross];
    }
    uint32_t _numLines;
    uint32_t _numCharsAcross;
    uint8_t* _pChars;
 };

 
class DisplayWindow
{
public:
    DisplayWindow()
    {
        _valid = false;
        tlx = tly = 0;
        width = height = 0;
        cellWidth = cellHeight = 0;
        xPixScale = yPixScale = 1;
        windowBackground = DISPLAY_FX_BLACK;
        windowForeground = DISPLAY_FX_WHITE;
        pFont = NULL;
        _cursorRow = 0;
        _cursorCol = 0;
        _cursorVisible = false;
        _pFontCache = NULL;
        _fontCacheUint32sPerLine = 1;
        _fontCacheUint32sPerChar = 1;
        _fontCacheError = 0;
        _pFrameBufError8 = NULL;
        _pFrameBufError32 = NULL;
    }

    virtual ~DisplayWindow()
    {
        delete _pFontCache;
    }

    uint32_t cols()
    {
        if (cellWidth <= 0)
            return 0;
        return width/cellWidth;
    }

    uint32_t rows()
    {
        if (cellHeight <= 0)
            return 0;
        return height/cellHeight;
    }
    void genFontAtCurRes(DISPLAY_FX_COLOUR screenFG, DISPLAY_FX_COLOUR screenBG);
    // TODO 2020 remove
    void debug();

public:
    bool _valid;
    uint32_t tlx;
    uint32_t width;
    uint32_t tly;
    uint32_t height;
    uint32_t cellWidth;
    uint32_t cellHeight;
    uint32_t xPixScale;
    uint32_t yPixScale;
    int32_t windowBackground;
    int32_t windowForeground;
    WgfxFont* pFont;
    uint32_t _fontCacheUint32sPerLine;
    uint32_t _fontCacheUint32sPerChar;
    int _fontCacheError;
    uint8_t* _pFrameBufError8;
    uint32_t* _pFrameBufError32;

    // Font rendered at current res
    uint32_t* _pFontCache;

    // Cursor
    uint32_t _cursorRow;
    uint32_t _cursorCol;
    bool _cursorVisible;
    // Make sure this is big enough for any font's character cell
    uint8_t _cursorBuffer[512];

    // Content
    // WindowContent _windowContent;
};

class DisplayFX
{
public:
    DisplayFX();
    ~DisplayFX();

    bool init(uint32_t displayWidth, uint32_t displayHeight);

    // TODO 2020 remove
    void debug()
    {
        _windows[_consoleWinIdx].debug();
    }

    // Screen
    void screenClear();
    void screenRectClear(uint32_t tlx, uint32_t tly, uint32_t width, uint32_t height);
    void screenBackground(DISPLAY_FX_COLOUR colour);

    // Window
    void windowSetup(uint32_t winIdx, 
                    int tlX, int tlY,
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour, 
                    int borderWidth, int borderColour);
    void windowClear(uint32_t winIdx);
    void windowPut(uint32_t winIdx, uint32_t col, uint32_t row, const uint8_t* pStr);
    void windowPut(uint32_t winIdx, uint32_t col, uint32_t row, uint32_t ch);
    void windowForeground(uint32_t winIdx, DISPLAY_FX_COLOUR colour);
    void windowBackground(uint32_t winIdx, DISPLAY_FX_COLOUR colour);
    void windowSetPixel(uint32_t winIdx, uint32_t x, uint32_t y, uint32_t value, DISPLAY_FX_COLOUR colour);

    // Console
    void consoleSetWindow(uint32_t consoleWinIdx);
    void consolePut(const char* pStr);
    void consolePut(int ch);
    void consolePut(const char* pBuffer, unsigned count);
    void consoleForeground(DISPLAY_FX_COLOUR colour);
    int consoleGetWidth();

    // Draw
    void drawHorizontal(uint32_t x, uint32_t y, uint32_t len, uint32_t colour);
    void drawVertical(uint32_t x, uint32_t y, uint32_t len, uint32_t colour);

    // RAW access
    void getFramebuffer(uint32_t winIdx, FrameBufferInfo& frameBufferInfo);

private:

    // Windows
    static const int DISPLAY_FX_MAX_WINDOWS = 5;
    DisplayWindow _windows[DISPLAY_FX_MAX_WINDOWS];

    // BCM frame buffer
    CBcmFrameBuffer* _pBCMFrameBuffer;

    // Screen
    uint32_t _screenWidth;
    uint32_t _screenHeight;
    uint32_t _pitch;
    uint32_t _size;
    uint8_t* _pRawFrameBuffer;
    DISPLAY_FX_COLOUR _screenBackground;
    DISPLAY_FX_COLOUR _screenForeground;

    // Console window
    uint32_t _consoleWinIdx;

    // Debug timer
    uint32_t _debugTimeLastMs;

    // Get framebuffer
    uint8_t* windowGetPFB(uint32_t winIdx, uint32_t col, uint32_t row);
    uint8_t* screenGetPFBXY(uint32_t x, uint32_t y);
    uint8_t* windowGetPFBXY(uint32_t winIdx, uint32_t x, uint32_t y);

    // Cursor
    void cursorCheck();
    void cursorRestore();
    void cursorRender();

    // Scroll
    void windowScroll(uint32_t winIdx, int32_t rows);

    // Access
    void screenReadCell(uint32_t winIdx, uint32_t col, uint32_t row, uint8_t* pCellBuf);
    void screenWriteCell(uint32_t winIdx, uint32_t col, uint32_t row, uint8_t* pCellBuf);
};