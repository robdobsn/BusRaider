// Bus Raider
// Rob Dobson 2019

#include "Display.h"
#include <string.h>

#include "DisplayFX.h"
#include "lowlib.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Layout
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const int DISPLAY_WIDTH = 1600;
static const int DISPLAY_HEIGHT = 900;

static const int DISPLAY_TARGET_BORDER = 8;
static const int DISPLAY_STATUS_LINES = 8;
static const int DISPLAY_STATUS_MARGIN = 10;
static const int DISPLAY_CONSOLE_MARGIN = 10;

static const int DISPLAY_TARGET_WIDTH = 1024;
static const int DISPLAY_TARGET_HEIGHT = DISPLAY_HEIGHT - (2 * DISPLAY_TARGET_BORDER);

static const int DISPLAY_WINDOW_TARGET = 0;
static const int DISPLAY_WINDOW_STATUS = 1;
static const int DISPLAY_WINDOW_CONSOLE = 2;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Display::Display()
{
    _displayStarted = false;
}

Display::~Display()
{
}

bool Display::init()
{
    // Init hardware
    _displayFX.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // Target machine window
    _displayFX.windowSetup(DISPLAY_WINDOW_TARGET, 
            0, 0, 
            DISPLAY_TARGET_WIDTH, DISPLAY_TARGET_HEIGHT, 
            8, 8, 
            1, 1,
            NULL, 
            -1, -1,
            DISPLAY_TARGET_BORDER, DISPLAY_FX_DARK_GRAY);

    // Status window
    _displayFX.windowSetup(DISPLAY_WINDOW_STATUS, 
        DISPLAY_TARGET_WIDTH + DISPLAY_TARGET_BORDER * 2 + DISPLAY_STATUS_MARGIN, 
        0,
        -1, -1, 
        -1, -1, 
        1, 1, 
        NULL, 
        -1, -1,
        0, DISPLAY_FX_DARK_GRAY);

    // Console window
    _displayFX.windowSetup(DISPLAY_WINDOW_CONSOLE, 
        DISPLAY_TARGET_WIDTH + DISPLAY_TARGET_BORDER * 2 + DISPLAY_CONSOLE_MARGIN, 
        DISPLAY_STATUS_LINES * __systemFont.cellY,
        -1, -1, 
        -1, -1, 
        1, 1, 
        NULL, 
        -1, -1,
        0, DISPLAY_FX_DARK_GRAY);
    _displayFX.consoleSetWindow(DISPLAY_WINDOW_CONSOLE);

    // Clear current strings
    for (int i = 0; i < STATUS_FIELD_NUM_ELEMENTS; i++)
        _statusFieldStrings[i][0] = 0;

    // Started ok
    _displayStarted = true;

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Display::statusPut(int statusElement, int statusType, const char* pStr)
{
    static struct
    {
        int x;
        int y;
    } fieldPositions[] =
    {
        { 0,                        0 }, // Pi Version and credits
        { 0,                        1 }, // Links
        { 0,                        2 }, // ESP version
        { DISPLAY_STATUS_INDENT,    2 }, // IP addr
        { 0,                        3 }, // Cur machine
        { DISPLAY_STATUS_INDENT,    3 }, // Clock speed
        { 0,                        4 }, // Bus access
        { DISPLAY_STATUS_INDENT-2,  4 }, // Refresh
        { 0,                        5 }, // Keyboard
        { 30,                       5 }, // Asserts
        { 0,                        6 }, // Asserts
    };

    // Check for changes
    if (statusElement >= STATUS_FIELD_NUM_ELEMENTS)
        return;
    if (strcmp(_statusFieldStrings[statusElement], pStr) == 0)
        return;
    strncpy(_statusFieldStrings[statusElement], pStr, MAX_STATUS_FIELD_STRLEN-1);
    int x = fieldPositions[statusElement].x;
    int y = fieldPositions[statusElement].y;
    windowForeground(DISPLAY_WINDOW_STATUS, 
            (statusType == STATUS_FAIL) ? DISPLAY_FX_RED : (statusType == STATUS_NORMAL) ? DISPLAY_FX_YELLOW : DISPLAY_FX_GREEN);
    windowWrite(DISPLAY_WINDOW_STATUS, x, y, (const uint8_t*)pStr);
    windowForeground(DISPLAY_WINDOW_STATUS, DISPLAY_FX_WHITE);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Display::targetLayout(
                int pixX, int pixY, 
                int cellX, int cellY, 
                int xScale, int yScale,
                WgfxFont* pFont, 
                int foreColour, int backColour)
{
    if (!_displayStarted)
        return;

    _displayFX.screenRectClear(0, 0, DISPLAY_TARGET_WIDTH + DISPLAY_TARGET_BORDER * 2, DISPLAY_TARGET_HEIGHT + DISPLAY_TARGET_BORDER * 2);
    _displayFX.windowSetup(DISPLAY_WINDOW_TARGET, 
            0, 0, 
            pixX, pixY, 
            cellX, cellY, 
            xScale, yScale,
            pFont, 
            foreColour, backColour, 
            DISPLAY_TARGET_BORDER, DISPLAY_FX_DARK_GRAY);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Windows
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Display::windowForeground(uint32_t winIdx, DISPLAY_FX_COLOUR colour)
{
    if (!_displayStarted)
        return;
    _displayFX.windowForeground(winIdx, colour);
}

void Display::windowBackground(uint32_t winIdx, DISPLAY_FX_COLOUR colour)
{
    if (!_displayStarted)
        return;
    _displayFX.windowBackground(winIdx, colour);
}

void Display::windowWrite(uint32_t winIdx, uint32_t col, uint32_t row, const uint8_t* pStr)
{
    if (!_displayStarted)
        return;
    _displayFX.windowPut(winIdx, col, row, pStr);
}

void Display::windowWrite(uint32_t winIdx, uint32_t col, uint32_t row, uint32_t ch)
{
    if (!_displayStarted)
        return;
    _displayFX.windowPut(winIdx, col, row, ch);
}

void Display::windowSetPixel(uint32_t winIdx, uint32_t x, uint32_t y, uint32_t value, DISPLAY_FX_COLOUR colour)
{
    if (!_displayStarted)
        return;
    _displayFX.windowSetPixel(winIdx, x, y, value, colour);
}

void Display::foreground(DISPLAY_FX_COLOUR colour)
{
    windowForeground(DISPLAY_WINDOW_TARGET, colour);
}


void Display::background(DISPLAY_FX_COLOUR colour)
{
    windowBackground(DISPLAY_WINDOW_TARGET, colour);
}

void Display::write(uint32_t col, uint32_t row, const uint8_t* pStr)
{
    windowWrite(DISPLAY_WINDOW_TARGET, col, row, pStr);
}

void Display::write(uint32_t col, uint32_t row, uint32_t ch)
{
    windowWrite(DISPLAY_WINDOW_TARGET, col, row, ch);
}

void Display::setPixel(uint32_t x, uint32_t y, uint32_t value, DISPLAY_FX_COLOUR colour)
{
    windowSetPixel(DISPLAY_WINDOW_TARGET, x, y, value, colour);
}

void Display::getFrameBufferInfo(FrameBufferInfo& frameBufferInfo)
{
    _displayFX.getFramebuffer(DISPLAY_WINDOW_TARGET, frameBufferInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Console
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Display::consolePut(const char* pStr)
{
    if (!_displayStarted)
        return;
    _displayFX.consolePut(pStr);
}

void Display::consolePut(uint32_t ch)
{
    if (!_displayStarted)
        return; 
    _displayFX.consolePut(ch);
}

void Display::consolePut(const char* pBuffer, unsigned count)
{
    if (!_displayStarted)
        return;
    _displayFX.consolePut(pBuffer, count);
}

void Display::consoleForeground(DISPLAY_FX_COLOUR colour)
{
    if (!_displayStarted)
        return;
    _displayFX.consoleForeground(colour);
}

int Display::consoleGetWidth()
{
    if (!_displayStarted)
        return 0;
    return _displayFX.consoleGetWidth();
}

void Display::logDebug(const char* pSeverity, const char* pSource, const char* pMsg)
{
    consolePut(pSeverity);
    consolePut(":");
    // Time
// #define INCLUDE_MICROS_TIME 1
#ifdef INCLUDE_MICROS_TIME
    char timeStr[20];
    ee_sprintf(timeStr, "%u ", micros());
    consolePut(timeStr);
    consolePut(" ");
#endif
    consolePut(pSource);
    consolePut(": ");
    consolePut(pMsg);
    consolePut("\n");
}    

// Implementation of CDevice base class
int Display::Write(const void *pBuffer, size_t nCount)
{
    consolePut((const char*)pBuffer, nCount);
    return nCount;
}
