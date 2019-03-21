// Bus Raider
// Rob Dobson 2019

#pragma once

#include "wgfxfont.h"
#include "stdint.h"
#include "DisplayFX.h"

class Display
{
public:
    enum STATUS_FIELD_ELEMENTS
    {
        STATUS_FIELD_PI_VERSION,
        STATUS_FIELD_LINKS,
        STATUS_FIELD_ESP_VERSION,
        STATUS_FIELD_IP_ADDR,
        STATUS_FIELD_CUR_MACHINE,
        STATUS_FIELD_MACHINES,
        STATUS_FIELD_BUS_ACCESS,
        STATUS_FIELD_REFRESH_RATE,
        STATUS_FIELD_KEYBOARD,
        STATUS_FIELD_ASSERTS
    };

    static const int STATUS_NORMAL = 0;
    static const int STATUS_FAIL = 1;

public:
    Display();
    ~Display();

    bool init();

    void masterLayout(int targetWindowWidth, int targetWindowHeight,
            int targetWindowBoarderPix);

    // Target
    void targetLayout(int tlX, int tlY,
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour, 
                    int borderWidth, int borderColour);

    // Status
    void statusPut(int statusElement, int statusType, const char* pStr);

    // Window
    void windowForeground(int winIdx, DISPLAY_FX_COLOUR colour);
    void windowWrite(int winIdx, int col, int row, const char* pStr);

    // Console
    void consolePut(const char* pStr);
    void consolePut(int ch);
    void consoleForeground(DISPLAY_FX_COLOUR colour);
    int consoleGetWidth();

private:

    // DisplayFX
    DisplayFX _displayFX;

    // Layout
    static const int TARGET_WINDOW_BORDER_PIX = 5;

    // Flag
    bool _displayStarted;
};