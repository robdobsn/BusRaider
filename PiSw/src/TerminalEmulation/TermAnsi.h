// Bus Raider
// Rob Dobson 2019
// Portions Copyright (c) 2017 Rob King

#pragma once

#include "TermEmu.h"

#define TMT_KEY_UP             "\033[A"
#define TMT_KEY_DOWN           "\033[B"
#define TMT_KEY_RIGHT          "\033[C"
#define TMT_KEY_LEFT           "\033[D"
#define TMT_KEY_HOME           "\033[H"
#define TMT_KEY_END            "\033[Y"
#define TMT_KEY_INSERT         "\033[L"
#define TMT_KEY_BACKSPACE      "\x08"
#define TMT_KEY_ESCAPE         "\x1b"
#define TMT_KEY_BACK_TAB       "\033[Z"
#define TMT_KEY_PAGE_UP        "\033[V"
#define TMT_KEY_PAGE_DOWN      "\033[U"
#define TMT_KEY_F1             "\033OP"
#define TMT_KEY_F2             "\033OQ"
#define TMT_KEY_F3             "\033OR"
#define TMT_KEY_F4             "\033OS"
#define TMT_KEY_F5             "\033OT"
#define TMT_KEY_F6             "\033OU"
#define TMT_KEY_F7             "\033OV"
#define TMT_KEY_F8             "\033OW"
#define TMT_KEY_F9             "\033OX"
#define TMT_KEY_F10            "\033OY"

class TermAnsi : public TermEmu
{
public:

    virtual void init(uint32_t cols, uint32_t rows);
    virtual void putChar(uint32_t ch);
    virtual void reset();

    enum tmt_color_t {
        TMT_COLOR_DEFAULT = -1,
        TMT_COLOR_BLACK = 1,
        TMT_COLOR_RED,
        TMT_COLOR_GREEN,
        TMT_COLOR_YELLOW,
        TMT_COLOR_BLUE,
        TMT_COLOR_MAGENTA,
        TMT_COLOR_CYAN,
        TMT_COLOR_WHITE,
        TMT_COLOR_MAX
    };

    union tmt_attrs {
        struct {
            bool bold : 1;
            bool dim : 1;
            bool underline : 1;
            bool blink : 1;
            bool reverse : 1;
            bool invisible :1 ;
        };
        uint32_t attrs;
    };

    static const uint32_t DEFAULT_ATTRIBS = 0;

    enum tmt_msg_t{
        TMT_MSG_MOVED,
        TMT_MSG_UPDATE,
        TMT_MSG_ANSWER,
        TMT_MSG_BELL,
        TMT_MSG_CURSOR
    } ;

    enum tmt_vt_state {S_NUL, S_ESC, S_ARG};
    tmt_vt_state _vtState;

private:
    void vtCallback([[maybe_unused]]tmt_msg_t msg, [[maybe_unused]]const char* str)
    {

    }
    bool handleAnsiChar(uint8_t ch);
    void writeCharAtCurs(int ch);
    void fixcursor();
    void consumearg();
    void resetparser();
    void scrollUp(size_t startRow, size_t n);
    void scrollDown(size_t startRow, size_t n);
    void dirtylines(size_t start, size_t end);
    void clearlines(size_t rowIdx, size_t numRows);
    void clearline(size_t lineIdx, size_t startCol, size_t endCol);
    void ed();
    void el();
    void dch();
    void ich();
    void rep();
    void sgr();
    void dsr();

    static const int MAX_ANSI_PARAMS = 8;
    size_t _ansiParams[MAX_ANSI_PARAMS];   
    size_t _ansiParamsNum;
    size_t _arg;

    bool _vtIgnored;

    static const int MAX_TABS_LINE_LEN = 250;
    TermChar _tabLine[MAX_TABS_LINE_LEN];

    static const int MAX_ANSI_TERMINAL_LINES = 500;
    bool _lineDirty[MAX_ANSI_TERMINAL_LINES];

    TermChar _curAttrs;
    TermChar _oldAttrs;

    TermCursor _oldCursor;
};
