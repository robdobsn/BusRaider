// Bus Raider
// Rob Dobson 2019

#include "TermEmu.h"
#include "stddef.h"

TermEmu::TermEmu()
{
    _pCharBuffer = NULL;
    _cols = 80;
    _rows = 25;
    _charBufferSize = 0;
}

TermEmu::~TermEmu()
{
    if (_pCharBuffer)
        delete [] _pCharBuffer;
}

void TermEmu::init(uint32_t cols, uint32_t rows)
{
    if (_pCharBuffer)
        delete [] _pCharBuffer;
    _pCharBuffer = new TermChar [cols*rows];
    _charBufferSize = cols*rows;
    _cols = cols;
    _rows = rows;
    _cursor.clear();
}

void TermEmu::putChar(uint32_t ch)
{

}

void TermEmu::reset()
{
}
