/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RingBufferRTOS
// Template-based ring bufffer
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RingBufferPosnRTOS.h"
#include <vector>

template<typename ElemT, uint32_t bufferSize>
class RingBufferRTOS
{
public:
    RingBufferRTOS() :
        _bufPos(bufferSize)
    {
    }

    bool put(const ElemT& elem)
    {
        if (_bufPos.canPut())
        {
            _buffer[_bufPos.posToPut()] = elem;
            _bufPos.hasPut();
            return true;
        }
        return false;
    }

    bool get(ElemT& elem)
    {
        if (_bufPos.canGet())
        {
            elem = _buffer[_bufPos.posToGet()];
            _bufPos.hasGot();
            return true;
        }
        return false;
    }

    void clear()
    {
        _bufPos.clear();
    }

    uint32_t count()
    {
        return _bufPos.count();
    }

    uint32_t maxLen()
    {
        return bufferSize;
    }

private:
    RingBufferPosnRTOS _bufPos;
    ElemT _buffer[bufferSize];
};

