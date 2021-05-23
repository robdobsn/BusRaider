/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef ESP8266

#include <WString.h>
#include "RdWebResponder.h"
#include "RdWebRequestParams.h"

class FileSystemChunker;
class RdWebHandler;

class RdWebResponderFile : public RdWebResponder
{
public:
    RdWebResponderFile(const String& filePath, RdWebHandler* pWebHandler, const RdWebRequestParams& params);
    virtual ~RdWebResponderFile();

    // Handle inbound data
    virtual bool handleData(const uint8_t* pBuf, uint32_t dataLen) override final;

    // Start responding
    virtual bool startResponding(RdWebConnection& request) override final;

    // Get response next
    virtual uint32_t getResponseNext(uint8_t* pBuf, uint32_t bufMaxLen) override final;

    // Get content type
    virtual const char* getContentType() override final;

    // Get content length (or -1 if not known)
    virtual int getContentLength() override final;

    // Leave connection open
    virtual bool leaveConnOpen() override final;

    // Get responder type
    virtual const char* getResponderType() override final
    {
        return "FILE";
    }

    // Ready for data
    virtual bool readyForData() override final
    {
        return true;
    }

private:
    String _filePath;
    RdWebHandler* _pWebHandler;
    FileSystemChunker* _pChunker;
    RdWebRequestParams _reqParams;
    uint32_t _fileLength;
};

#endif
