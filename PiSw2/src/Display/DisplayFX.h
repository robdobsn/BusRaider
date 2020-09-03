// Bus Raider
// Rob Dobson 2019

#pragma once

#include "DisplayBase.h"
#include "wgfxfont.h"
#include "stdint.h"
#include "stddef.h"
#include <circle/bcmframebuffer.h>
#include <circle/types.h>

extern "C" WgfxFont __systemFont;

#define DEPTH	16		// can be: 8, 16 or 32

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
    void setSize(int numLines, int numCharsAcross)
    {
        if (_pChars)
            delete [] _pChars;
        _pChars = NULL;
        _numLines = numLines;
        _numCharsAcross = numCharsAcross;
        _pChars = new uint8_t[_numLines * _numCharsAcross];
    }
    int _numLines;
    int _numCharsAcross;
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
    }

    int cols()
    {
        if (cellWidth <= 0)
            return 0;
        return width/cellWidth;
    }

    int rows()
    {
        if (cellHeight <= 0)
            return 0;
        return height/cellHeight;
    }

public:
    bool _valid;
    int tlx;
    int width;
    int tly;
    int height;
    int cellWidth;
    int cellHeight;
    int xPixScale;
    int yPixScale;
    int windowBackground;
    int windowForeground;
    WgfxFont* pFont;

    // Cursor
    int _cursorRow;
    int _cursorCol;
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

    bool init(int displayWidth, int displayHeight);

    // Screen
    void screenClear();
    void screenRectClear(int tlx, int tly, int width, int height);
    void screenBackground(DISPLAY_FX_COLOUR colour);

    // Window
    void windowSetup(int winIdx, int tlX, int tlY,
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour, 
                    int borderWidth, int borderColour);
    void windowClear(int winIdx);
    void windowPut(int winIdx, int col, int row, const char* pStr);
    void windowPut(int winIdx, int col, int row, int ch);
    void windowForeground(int winIdx, DISPLAY_FX_COLOUR colour);
    void windowBackground(int winIdx, DISPLAY_FX_COLOUR colour);
    void windowSetPixel(int winIdx, int x, int y, int value, DISPLAY_FX_COLOUR colour);

    // Console
    void consoleSetWindow(int consoleWinIdx);
    void consolePut(const char* pStr);
    void consolePut(int ch);
    void consoleForeground(DISPLAY_FX_COLOUR colour);
    int consoleGetWidth();

    // Draw
    void drawHorizontal(int x, int y, int len, int colour);
    void drawVertical(int x, int y, int len, int colour);

    // RAW access
    void getFramebuffer(int winIdx, FrameBufferInfo& frameBufferInfo);

private:

    // Windows
    static const int DISPLAY_FX_MAX_WINDOWS = 5;
    DisplayWindow _windows[DISPLAY_FX_MAX_WINDOWS];

    // BCM frame buffer
    CBcmFrameBuffer* _pBCMFrameBuffer;

    // Screen
    int _screenWidth;
    int _screenHeight;
    int _pitch;
    int _size;
    uint8_t* _pRawFrameBuffer;
    DISPLAY_FX_COLOUR _screenBackground;
    DISPLAY_FX_COLOUR _screenForeground;

    // Console window
    int _consoleWinIdx;

    // Get framebuffer
    uint8_t* windowGetPFB(int winIdx, int col, int row);
    uint8_t* screenGetPFBXY(int x, int y);
    uint8_t* windowGetPFBXY(int winIdx, int x, int y);

    // Cursor
    void cursorCheck();
    void cursorRestore();
    void cursorRender();

    // Scroll
    void windowScroll(int winIdx, int rows);

    // Access
    void screenReadCell(int winIdx, int col, int row, uint8_t* pCellBuf);
    void screenWriteCell(int winIdx, int col, int row, uint8_t* pCellBuf);
};