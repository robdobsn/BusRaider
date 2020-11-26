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
        STATUS_FIELD_ASSERTS,
        STATUS_FIELD_FILE_STATUS,
        STATUS_FIELD_NUM_ELEMENTS
    };

    static const int STATUS_NORMAL = 0;
    static const int STATUS_FAIL = 1;
    static const int STATUS_HILITE = 2;

public:
    Display();
    ~Display();

    bool init();

    // TODO 2020 remove
    virtual void debug()
    {
        _displayFX.debug();
    }

    // Target
    virtual void targetLayout(
                    int pixX, int pixY, 
                    int cellX, int cellY, 
                    int xScale, int yScale,
                    WgfxFont* pFont, 
                    int foreColour, int backColour) override;

    // Status
    void statusPut(int statusElement, int statusType, const char* pStr);

    // Window
    void windowForeground(uint32_t winIdx, DISPLAY_FX_COLOUR colour);
    void windowBackground(uint32_t winIdx, DISPLAY_FX_COLOUR colour);
    void windowWrite(uint32_t winIdx, uint32_t col, uint32_t row, const uint8_t* pStr);
    void windowWrite(uint32_t winIdx, uint32_t col, uint32_t row, uint32_t ch);
    void windowSetPixel(uint32_t winIdx, uint32_t x, uint32_t y, uint32_t value, DISPLAY_FX_COLOUR colour);

    // Console
    void consolePut(const char* pStr);
    void consolePut(uint32_t ch);
    void consolePut(const char* pBuffer, unsigned count);
    void consoleForeground(DISPLAY_FX_COLOUR colour);
    int consoleGetWidth();
 
    // Log debug
    void logDebug(const char* pSeverity, const char* pSource, const char* pMsg);

    // Target window
    virtual void foreground(DISPLAY_FX_COLOUR colour) override;
    virtual void background(DISPLAY_FX_COLOUR colour) override;
    virtual void write(uint32_t col, uint32_t row, const uint8_t* pStr) override;
    virtual void write(uint32_t col, uint32_t row, uint32_t ch) override;
    virtual void setPixel(uint32_t x, uint32_t y, uint32_t value, DISPLAY_FX_COLOUR colour) override;

    // Implementations of CDevice base class
    int Write(const void *pBuffer, size_t nCount);

    // RAW access
    virtual void getFrameBufferInfo(FrameBufferInfo& frameBufferInfo) override;

	/// \brief Displays rotating symbols in the upper right corner of the screen
	/// \param nIndex Index of the rotor to be displayed (0..3)
	/// \param nCount Phase (angle) of the current rotor symbol (0..3)
	void rotor(unsigned nIndex, unsigned nCount)
    {
    }

private:

    // DisplayFX
    DisplayFX _displayFX;

    // Layout
    static const int TARGET_WINDOW_BORDER_PIX = 5;

    // Flag
    bool _displayStarted;

    // Current status field values
    static const int MAX_STATUS_FIELD_STRLEN = 100;
    char _statusFieldStrings[STATUS_FIELD_NUM_ELEMENTS][MAX_STATUS_FIELD_STRLEN];
};