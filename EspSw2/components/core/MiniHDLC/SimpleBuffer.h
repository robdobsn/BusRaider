/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SimpleBuffer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <stddef.h>

#ifdef __circle__
#define USE_FIXED_LENGTH_BUFFER_LEN 10000
#endif

#ifndef USE_FIXED_LENGTH_BUFFER_LEN
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
        _bufMaxLen = maxFrameLen;
#endif
    }

    virtual ~SimpleBuffer()
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        return;
#endif
    }

    void clear()
    {
        _buffer.clear();
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
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        return false;
#else
        if (size > _bufMaxLen)
            return false;
        _buffer.resize(size);
        return _buffer.size() == size;
#endif
    }

    bool setAt(unsigned idx, uint8_t val)
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        if (idx >= _bufMaxLen)
            return false;
        _buffer[idx] = val;
        return true;   
#else
        if (idx >= _bufMaxLen)
            return false;
        if (idx >= _buffer.size())
            _buffer.resize(idx+1);
        if (idx >= _buffer.size())
                return false;
        _buffer[idx] = val;
        return true;
#endif
    }

    uint8_t getAt(unsigned idx) const
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        if (idx >= _bufMaxLen)
            return 0;
        return _buffer[idx];
#else
        if (idx >= _buffer.size())
            return 0;
        return _buffer[idx];
#endif
    }

    uint8_t* data()
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        return _buffer;
#else
        return _buffer.data();
#endif
    }

    unsigned size()
    {
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
        return _bufMaxLen;
#else
        return _buffer.size();
#endif
    }

private:
#ifdef USE_FIXED_LENGTH_BUFFER_LEN
    uint8_t _buffer[USE_FIXED_LENGTH_BUFFER_LEN];
#else
    std::vector<uint8_t> _buffer;
#endif
    unsigned _bufMaxLen;
};
