// Bus Raider
// Rob Dobson 2019

#pragma once

#include "TermEmu.h"

class TermH19 : public TermEmu
{
public:
    TermH19();
    virtual ~TermH19();
    virtual void init(uint32_t cols, uint32_t rows) override;
    virtual void putChar(uint32_t ch) override;
    virtual void reset() override;

private:
    enum InputMode
    {
        Normal,
        Escape,
        DCA_1,
        DCA_2,
        SetMode,
        ResetMode
    };
    InputMode mode_m;

    uint8_t             sw401_m;
    uint8_t             sw402_m;

    // display modes
    bool             reverseVideo_m;
    bool             graphicMode_m;
    bool             insertMode_m;
    bool             line25_m;
    bool             holdScreen_m;
    bool             wrapEOL_m;
    bool             autoLF_m;
    bool             autoCR_m;

    bool             keyboardEnabled_m;
    bool             keyClick_m;
    bool             keypadShifted_m;
    bool             altKeypadMode_m;

    unsigned int     saveX_m, saveY_m;

    bool _ansiMode;
    uint32_t _rowsMain;

    virtual void setAnsiMode(bool on);

    // character handling
    virtual void processBS();
    virtual void processTAB();

    inline virtual void scroll()
    {
        for (unsigned int y = 0; y < (_rowsMain - 1); ++y)
        {
            for (unsigned int x = 0; x < _cols; ++x)
            {
                getTermChar(y*_cols+x) = getTermChar((y + 1)*_cols+x);
            }
        }

        eraseLine(_rowsMain - 1);
    };

    //
    // internal routines to handle ESC sequences.
    //

    // Cursor
    virtual void cursorHome();
    virtual void cursorForward();
    // virtual void cursorBackward(); // unneeded same as processBS.
    virtual void cursorDown();
    virtual void cursorUp();
    virtual void reverseIndex();
    virtual void cursorPositionReport();
    virtual void saveCursorPosition();
    virtual void restoreCursorPosition();

    // Erasing and Editing
    virtual void clearDisplay();
    virtual void eraseBOD();
    virtual void eraseEOP();
    virtual void eraseEL();
    virtual void eraseBOL();
    virtual void eraseEOL();
    virtual void insertLine();
    virtual void deleteLine();
    virtual void deleteChar();
    virtual void eraseLine(unsigned int line);

    //
    virtual void processLF();
    virtual void processCR();
    virtual void processEnableLine25();

    virtual void transmitLines(int start, int end)
    {}
    virtual void transmitLine25()
    {}
    virtual void transmitPage()
    {}
    virtual void displayCharacter(unsigned int ch);

    inline bool onLine25()
    {
        return (_cursor._row == (_rows - 1));
    };
    virtual void bell(void)
    {}

    const uint8_t RefreshRate_c  = 0x80;
    const uint8_t KeypadMode_c   = 0x40;
    const uint8_t TerminalMode_c = 0x20;
    const uint8_t AutoCR_c       = 0x10;
    const uint8_t AutoLF_c       = 0x08;
    const uint8_t WrapEOL_c      = 0x04;
    const uint8_t KeyClick_c     = 0x02;
    const uint8_t BlockCursor    = 0x01;

    void setSW401(uint8_t sw401);
    void setSW402(uint8_t sw402);

};
