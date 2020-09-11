/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SimpleBuffer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stddef.h>

#define USE_STD_VECTOR_BUFFERS

#ifdef USE_STD_VECTOR_BUFFERS
#include <vector>
#endif

class SimpleBuffer
{
public:
    static const unsigned DEFAULT_MAX_LEN = 5000;
    SimpleBuffer(unsigned maxFrameLen = DEFAULT_MAX_LEN)
    {
#ifndef USE_STD_VECTOR_BUFFERS
        _pBuffer = NULL;
        _bufferSize = 0;
#endif
        _bufMaxLen = maxFrameLen;
    }

    virtual ~SimpleBuffer()
    {
#ifndef USE_STD_VECTOR_BUFFERS
        if (_pBuffer)
            delete [] _pBuffer;
#endif
    }

    void setMaxLen(unsigned maxLen)
    {
        _bufMaxLen = maxLen;
    }

    bool resize(unsigned size)
    {
        if (size > _bufMaxLen)
            return false;
#ifdef USE_STD_VECTOR_BUFFERS
        _buffer.resize(size);
        return _buffer.size() == size;
#else
        if (_pBuffer)
        {
            delete [] _pBuffer;
            _pBuffer = NULL;
            _bufferSize = 0;
        }
        _pBuffer = new uint8_t[size];
        if (!_pBuffer)
            return false;
        _bufferSize = size;
        return true;
#endif
    }

    bool setAt(unsigned idx, uint8_t val)
    {
#ifdef USE_STD_VECTOR_BUFFERS
        if (idx >= _bufMaxLen)
            return false;
        if (idx >= _buffer.size())
            _buffer.resize(idx+1);
        if (idx >= _buffer.size())
            return false;
        _buffer[idx] = val;
        return true;   
#else
        if (idx >= _bufMaxLen)
            return false;
        if ((idx >= _bufferSize) && _pBuffer)
        {
            delete [] _pBuffer;
            _pBuffer = NULL;
            _bufferSize = 0;
        }
        if (!_pBuffer)
        {
            _pBuffer = new uint8_t[_bufMaxLen];
            if (!_pBuffer)
                return false;
            _bufferSize = _bufMaxLen;
        }
        _pBuffer[idx] = val;
        return true;
#endif
    }

    uint8_t getAt(unsigned idx) const
    {
#ifdef USE_STD_VECTOR_BUFFERS
        if (idx >= _buffer.size())
            return 0;
        return _buffer[idx];
#else
        if (!_pBuffer || (idx >= _bufMaxLen))
            return 0;
        return (_pBuffer[idx]);
#endif
    }

    uint8_t* data()
    {
#ifdef USE_STD_VECTOR_BUFFERS
        return _buffer.data();
#else
        return _pBuffer;
#endif
    }

    unsigned size()
    {
#ifdef USE_STD_VECTOR_BUFFERS
        return _buffer.size();
#else
        if (!_pBuffer)
            return 0;
        return _bufferSize;
#endif
    }

private:
#ifdef USE_STD_VECTOR_BUFFERS
    std::vector<uint8_t> _buffer;
#else
    uint8_t* _pBuffer;
    unsigned _bufferSize;
#endif
    unsigned _bufMaxLen;
};
