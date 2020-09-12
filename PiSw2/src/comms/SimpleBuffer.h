/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SimpleBuffer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stddef.h>

// #define USE_STD_VECTOR_BUFFERS
#define USE_FIXED_LENGTH_BUFFER_LEN 10000

#ifdef USE_STD_VECTOR_BUFFERS
#include <vector>
#endif

class SimpleBuffer
{
public:
    static const unsigned DEFAULT_MAX_LEN = 5000;
    SimpleBuffer(unsigned maxFrameLen = DEFAULT_MAX_LEN)
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        _bufMaxLen = USE_FIXED_LENGTH_BUFFER_LEN;
#else
#ifndef USE_STD_VECTOR_BUFFERS
        _pBuffer = NULL;
        _bufferSize = 0;
#endif
        _bufMaxLen = maxFrameLen;
#endif
    }

    virtual ~SimpleBuffer()
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        return;
#else
#ifndef USE_STD_VECTOR_BUFFERS
        if (_pBuffer)
            delete [] _pBuffer;
#endif
#endif
    }

    void setMaxLen(unsigned maxLen)
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        return;
#endif
        _bufMaxLen = maxLen;
    }

    bool resize(unsigned size)
    {
        if (size > _bufMaxLen)
            return false;
#ifdef USE_STD_VECTOR_BUFFERS
        _buffer.resize(size);
        return _buffer.size() == size;
#elif USE_FIXED_LENGTH_BUFFER_LEN
        return false;
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
#elif USE_FIXED_LENGTH_BUFFER_LEN
        if (idx >= _bufMaxLen)
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
#elif USE_FIXED_LENGTH_BUFFER_LEN
        if (idx >= _bufMaxLen)
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
#elif USE_FIXED_LENGTH_BUFFER_LEN
        return _buffer;
#else
        return _pBuffer;
#endif
    }

    unsigned size()
    {
#ifdef USE_STD_VECTOR_BUFFERS
        return _buffer.size();
#elif USE_FIXED_LENGTH_BUFFER_LEN
        return _bufMaxLen;
#else
        if (!_pBuffer)
            return 0;
        return _bufferSize;
#endif
    }

private:
#ifdef USE_STD_VECTOR_BUFFERS
    std::vector<uint8_t> _buffer;
#elif USE_FIXED_LENGTH_BUFFER_LEN
    uint8_t _buffer[USE_FIXED_LENGTH_BUFFER_LEN];
#else
    uint8_t* _pBuffer;
    unsigned _bufferSize;
#endif
    unsigned _bufMaxLen;
};
