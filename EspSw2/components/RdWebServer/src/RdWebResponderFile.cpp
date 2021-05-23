/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ESP8266

#include "RdWebResponderFile.h"
#include "Logger.h"
#include "FileSystemChunker.h"

static const char *MODULE_PREFIX = "RdWebRespFile";

// #define DEBUG_RESPONDER_FILE
// #define DEBUG_RESPONDER_FILE_CONTENTS
// #define DEBUG_RESPONDER_FILE_START_END

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponderFile::RdWebResponderFile(const String& filePath, RdWebHandler* pWebHandler, 
                const RdWebRequestParams& params)
    : _reqParams(params)
{
    _filePath = filePath;
    _pWebHandler = pWebHandler;
    _pChunker = new FileSystemChunker();
}

RdWebResponderFile::~RdWebResponderFile()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle inbound data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderFile::handleData(const uint8_t* pBuf, uint32_t dataLen)
{
#ifdef DEBUG_RESPONDER_FILE
    LOG_I(MODULE_PREFIX, "handleData len %d", dataLen);
#endif
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start responding
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderFile::startResponding(RdWebConnection& request)
{
    if (!_pChunker)
        return false;
    _isActive = _pChunker->start(_filePath, _reqParams.getMaxSendSize(), false);
#ifdef DEBUG_RESPONDER_FILE_START_END
    LOG_I(MODULE_PREFIX, "startResponding isActive %d", _isActive);
#endif
    return _isActive;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get response next
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RdWebResponderFile::getResponseNext(uint8_t* pBuf, uint32_t bufMaxLen)
{
    if (!_pChunker)
        return 0;
    uint32_t readLen = 0;
    bool finalChunk = false;
    if (!_pChunker->next(pBuf, bufMaxLen, readLen, finalChunk))
    {
        _isActive = false;
        LOG_W(MODULE_PREFIX, "getResponseNext failed");
        return 0;
    }
    
    // Check if done
    if (finalChunk)
    {
        _isActive = false;
#ifdef DEBUG_RESPONDER_FILE_START_END
        LOG_I(MODULE_PREFIX, "getResponseNext endOfFile readLen %d", readLen);
#endif
    }
#ifdef DEBUG_RESPONDER_FILE_CONTENTS
    LOG_I(MODULE_PREFIX, "getResponseNext readLen %d isActive %d", readLen, _isActive);
#endif
    return readLen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get content type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* RdWebResponderFile::getContentType()
{
    if (_filePath.endsWith(".html"))
        return "text/html";
    else if (_filePath.endsWith(".htm"))
        return "text/html";
    else if (_filePath.endsWith(".css"))
        return "text/css";
    else if (_filePath.endsWith(".json"))
        return "text/json";
    else if (_filePath.endsWith(".js"))
        return "application/javascript";
    else if (_filePath.endsWith(".png"))
        return "image/png";
    else if (_filePath.endsWith(".gif"))
        return "image/gif";
    else if (_filePath.endsWith(".jpg"))
        return "image/jpeg";
    else if (_filePath.endsWith(".ico"))
        return "image/x-icon";
    else if (_filePath.endsWith(".svg"))
        return "image/svg+xml";
    else if (_filePath.endsWith(".eot"))
        return "font/eot";
    else if (_filePath.endsWith(".woff"))
        return "font/woff";
    else if (_filePath.endsWith(".woff2"))
        return "font/woff2";
    else if (_filePath.endsWith(".ttf"))
        return "font/ttf";
    else if (_filePath.endsWith(".xml"))
        return "text/xml";
    else if (_filePath.endsWith(".pdf"))
        return "application/pdf";
    else if (_filePath.endsWith(".zip"))
        return "application/zip";
    else if (_filePath.endsWith(".gz"))
        return "application/x-gzip";
    return "text/plain";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get content length (or -1 if not known)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int RdWebResponderFile::getContentLength()
{
    if (!_pChunker)
        return 0;
    return _pChunker->getFileLen();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Leave connection open
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderFile::leaveConnOpen()
{
    return false;
}

#endif