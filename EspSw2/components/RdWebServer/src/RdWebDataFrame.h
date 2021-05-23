/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string.h>
#include <stdint.h>
#include <vector>

// Buffer for tx queue
class RdWebDataFrame
{
public:
    RdWebDataFrame()
    {
    }
    RdWebDataFrame(const uint8_t* pBuf, uint32_t bufLen)
    {
        frame.resize(bufLen);
        memcpy(frame.data(), pBuf, frame.size());
    }
    const uint8_t* getData()
    {
        return frame.data();
    }
    uint32_t getLen()
    {
        return frame.size();
    }
private:
    std::vector<uint8_t> frame;
};