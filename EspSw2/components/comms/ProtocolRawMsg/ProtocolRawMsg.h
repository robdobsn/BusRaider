/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRawMsg
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include <vector>
#include <RdJson.h>
#include <stdint.h>

class ProtocolRawMsg
{
public:
    ProtocolRawMsg()
    {
    }

    ProtocolRawMsg(const uint8_t* pBuf, uint32_t bufLen)
    {
        _cmdVector.assign(pBuf, pBuf+bufLen);
    }

    void clear()
    {
        _cmdVector.resize(0);
        _cmdVector.shrink_to_fit();
    }

    void setFromBuffer(const uint8_t* pBuf, uint32_t bufLen)
    {
        _cmdVector.assign(pBuf, pBuf+bufLen);
    }

    void setBufferSize(uint32_t bufSize)
    {
        _cmdVector.resize(bufSize);
    }

    void setPartBuffer(uint32_t startPos, const uint8_t* pBuf, uint32_t len)
    {
        uint32_t reqSize = startPos + len;
        if (_cmdVector.size() < reqSize)
            _cmdVector.resize(reqSize);
        memcpy(_cmdVector.data() + startPos, pBuf, len);
    }

    // Access to command vector
    const uint8_t* getBuf()
    {
        return _cmdVector.data();
    }
    uint32_t getBufLen()
    {
        return _cmdVector.size();
    }
    std::vector<uint8_t>& getCmdVector()
    {
        return _cmdVector;
    }

private:
    std::vector<uint8_t> _cmdVector;
};
