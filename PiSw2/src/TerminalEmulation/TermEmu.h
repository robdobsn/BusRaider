// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include "logging.h"
#include "DisplayBase.h"

class TermCursor
{
public:
    uint32_t _col;
    uint32_t _row;
    bool _updated;
    bool _off;
    bool _blockCursor;
    uint32_t _cursorChar;
    uint32_t _replacedChar;

    TermCursor()
    {
        clear();
    }

    void clear()
    {
        _col = 0;
        _row = 0;
        _updated = false;
        _off = false;
        _blockCursor = false;
        _cursorChar = '_';
        _replacedChar = ' ';
    }
};

class TermChar
{
public:

    static const uint8_t TERM_DEFAULT_FORE_COLOUR = DISPLAY_FX_WHITE;
    static const uint8_t TERM_DEFAULT_BACK_COLOUR = DISPLAY_FX_BLACK;
    static const uint8_t TERM_ATTR_INVALID_CODE = 0xff;

    TermChar()
    {
        clear();
    }
    void clear()
    {
        _charCode = ' ';
        _foreColour = TERM_DEFAULT_FORE_COLOUR;
        _backColour = TERM_DEFAULT_BACK_COLOUR;
        _attribs = 0;
    }
    bool equals(TermChar& otherCh)
    {
        return _charVal == otherCh._charVal;
    }

    static const uint8_t TERM_ATTR_BOLD = 0x01;
    static const uint8_t TERM_ATTR_DIM = 0x02;
    static const uint8_t TERM_ATTR_UNDERLINE = 0x04;
    static const uint8_t TERM_ATTR_BLINK = 0x08;
    static const uint8_t TERM_ATTR_REVERSE = 0x10;
    static const uint8_t TERM_ATTR_INVISIBLE = 0x20;

    union
    {
        #pragma pack(push, 1)
        struct
        {
            uint8_t _charCode;
            uint8_t _foreColour;
            uint8_t _backColour;
            uint8_t _attribs;
        };
        #pragma pack(pop)
        uint32_t _charVal;
    };
};

class TermEmu
{
public:
    static const int MAX_ROWS = 100;
    static const int MAX_COLS = 500;
    TermEmu();
    virtual ~TermEmu();
    virtual void init(uint32_t cols, uint32_t rows);
    virtual void putChar(uint32_t ch);
    virtual void reset();
    virtual bool hasChanged()
    {
        return _cursor._updated;
    }
    virtual void sendData(int ch)
    {
        LogWrite("TermEmu", LOG_DEBUG, "SendData %02x", ch);
    }

    void consoleLog( const char* pStr)
    {
        // LogWrite("TermEmu", LOG_DEBUG, "%s", pStr);
    }

    inline TermChar& getTermChar(uint32_t idx)
    {
        if (idx >= _charBufferSize)
            return _fallbackTermChar;
        return _pCharBuffer[idx];
    }

    //     const TermChar& getTermChar(uint32_t idx)
    // {
    //     static const TermChar defaultChar;
    //     if (idx >= _charBufferSize)
    //         return defaultChar;
    //     return _pCharBuffer[idx];
    // }

    // void setTermChar(uint32_t idx, TermChar& termChar)
    // {
    //     static TermChar defaultChar;
    //     if (idx >= _charBufferSize)
    //         return defaultChar;
    //     return _pCharBuffer[idx];
    // }

    
public:
    TermChar* _pCharBuffer;
    uint32_t _charBufferSize;
    uint32_t _cols;
    uint32_t _rows;
    TermCursor _cursor;
    TermChar _fallbackTermChar;
};
