// Bus Raider
// Rob Dobson 2019

#pragma once

#include "wgfxfont.h"
#include "stdint.h"
#include "DisplayBase.h"
#include "DisplayFX.h"

class Display : public DisplayBase
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
    static const int STATUS_HILITE = 2;

public:
    Display();
    ~Display();

    bool init();

    // Target
    void targetLayout(
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour);

    // Status
    void statusPut(int statusElement, int statusType, const char* pStr);

    // Window
    void windowForeground(int winIdx, DISPLAY_FX_COLOUR colour);
    void windowWrite(int winIdx, int col, int row, const char* pStr);
    void windowWrite(int winIdx, int col, int row, int ch);
    void windowSetPixel(int winIdx, int x, int y, int value, DISPLAY_FX_COLOUR colour);

    // Console
    void consolePut(const char* pStr);
    void consolePut(int ch);
    void consoleForeground(DISPLAY_FX_COLOUR colour);
    int consoleGetWidth();

    // Target window
    void foreground(DISPLAY_FX_COLOUR colour);
    void write(int col, int row, const char* pStr);
    void write(int col, int row, int ch);
    void setPixel(int x, int y, int value, DISPLAY_FX_COLOUR colour);

private:

    // DisplayFX
    DisplayFX _displayFX;

    // Layout
    static const int TARGET_WINDOW_BORDER_PIX = 5;

    // Flag
    bool _displayStarted;
};