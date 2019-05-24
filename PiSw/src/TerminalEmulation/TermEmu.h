// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdint.h>
#include "../System/logging.h"

class TermCursor
{
public:
    uint32_t _col;
    uint32_t _row;
    bool _updated;
    bool _off;
    bool _blockCursor;

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
    }
};

class TermChar
{
public:
    TermChar()
    {
        _charCode = ' ';
        _foreColour = 1;
        _backColour = 0;
    }
    uint8_t _charCode;
    uint8_t _foreColour;
    uint8_t _backColour;
};

class TermEmu
{
public:
    TermEmu();
    ~TermEmu();
    virtual void init(uint32_t cols, uint32_t rows);
    virtual void putChar(uint32_t ch);
    virtual void reset();
    virtual bool hasChanged()
    {
        return false;
    }
    virtual void sendData(int ch)
    {
        LogWrite("TermEmu", LOG_DEBUG, "SendData %02x", ch);
    }

    void consoleLog(const char* pStr)
    {
        LogWrite("TermEmu", LOG_DEBUG, "%s", pStr);
    }
    
public:
    TermChar* _pCharBuffer;
    uint32_t _cols;
    uint32_t _rows;
    TermCursor _cursor;

};
