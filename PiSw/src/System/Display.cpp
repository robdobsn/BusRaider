// Bus Raider
// Rob Dobson 2019

#include "Display.h"
#include "ee_printf.h"
#include <string.h>

#ifdef USE_WGFX
#include "wgfx.h"
#else
#include "DisplayFX.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Layout
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const int DISPLAY_WIDTH = 1600;
static const int DISPLAY_HEIGHT = 900;

static const int DISPLAY_TARGET_WIDTH = 1024;
static const int DISPLAY_TARGET_HEIGHT = 768;

static const int DISPLAY_TARGET_BORDER = 5;
static const int DISPLAY_STATUS_LINES = 7;
static const int DISPLAY_STATUS_MARGIN = 10;
static const int DISPLAY_CONSOLE_MARGIN = 10;
static const int DISPLAY_STATUS_INDENT = 40;

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

    // Setup basic window layout
    _displayFX.windowSetup(DISPLAY_WINDOW_STATUS, DISPLAY_TARGET_WIDTH + DISPLAY_TARGET_BORDER * 2 + DISPLAY_STATUS_MARGIN, 0,
        -1, -1, -1, -1, 1, 1, 
        NULL, -1, -1,
        0, 8);
    _displayFX.windowSetup(DISPLAY_WINDOW_CONSOLE, DISPLAY_TARGET_WIDTH + DISPLAY_TARGET_BORDER * 2 + DISPLAY_CONSOLE_MARGIN, 
        DISPLAY_STATUS_LINES * __systemFont.cellY,
        -1, -1, -1, -1, 1, 1, 
        NULL, -1, -1,
        0, 8);
    _displayFX.consoleSetWindow(DISPLAY_WINDOW_CONSOLE);

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
        { DISPLAY_STATUS_INDENT,    3 }, // Num machines
        { 0,                        4 }, // Bus access
        { DISPLAY_STATUS_INDENT-2,  4 }, // Refresh
        { 0,                        5 }, // Keyboard
        { DISPLAY_STATUS_INDENT,    5 }, // Asserts
    };
    int x = fieldPositions[statusElement].x;
    int y = fieldPositions[statusElement].y;
    windowForeground(DISPLAY_WINDOW_STATUS, statusType == STATUS_FAIL ? DISPLAY_FX_RED : DISPLAY_FX_YELLOW);
    windowWrite(DISPLAY_WINDOW_STATUS, x, y, pStr);
    windowForeground(DISPLAY_WINDOW_STATUS, DISPLAY_FX_WHITE);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Display::targetLayout(int tlX, int tlY,
                int pixX, int pixY, 
                int cellX, int cellY, 
                int xScale, int yScale,
                WgfxFont* pFont, 
                int foreColour, int backColour, 
                int borderWidth, int borderColour)
{
    if (!_displayStarted)
        return;

    _displayFX.windowSetup(DISPLAY_WINDOW_TARGET, tlX, tlY, pixX, pixY, cellX, cellY, xScale, yScale,
                pFont, foreColour, backColour, borderWidth, borderColour);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Windows
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Display::windowForeground(int winIdx, DISPLAY_FX_COLOUR colour)
{
    if (!_displayStarted)
        return;
    _displayFX.windowForeground(winIdx, colour);
}

void Display::windowWrite(int winIdx, int col, int row, const char* pStr)
{
    if (!_displayStarted)
        return;
#ifdef USE_WGFX
    wgfx_puts(winIdx, col, row, pStr);
#else
    _displayFX.windowPut(winIdx, col, row, pStr);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Console
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Display::consolePut(const char* pStr)
{
    if (!_displayStarted)
        return;
#ifdef USE_WGFX
    wgfx_console_putstring(pStr);
#else
    _displayFX.consolePut(pStr);
#endif
}

void Display::consolePut(int ch)
{
    if (!_displayStarted)
        return; 
#ifdef USE_WGFX
    wgfx_console_putchar(ch);
#else
    _displayFX.consolePut(ch);
#endif
}

void Display::consoleForeground(DISPLAY_FX_COLOUR colour)
{
    if (!_displayStarted)
        return;
#ifdef USE_WGFX
    wgfx_set_fg(colour);
#else
    _displayFX.consoleForeground(colour);
#endif
}

int Display::consoleGetWidth()
{
    if (!_displayStarted)
        return 0;
#ifdef USE_WGFX
    return wgfx_get_console_width();
#else
    return _displayFX.consoleGetWidth();
#endif
}
