// Bus Raider
// Rob Dobson 2019

#include "TermEmu.h"
#include "stddef.h"

TermEmu::TermEmu()
{
    _pCharBuffer = NULL;
    _cols = 80;
    _rows = 25;
}

TermEmu::~TermEmu()
{
    delete [] _pCharBuffer;
}

void TermEmu::init(uint32_t cols, uint32_t rows)
{
    delete [] _pCharBuffer;
    _pCharBuffer = new TermChar [cols*rows];
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