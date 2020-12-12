/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdWebResponderRestAPI.h"
#include <RestAPIEndpointDef.h>
#include <Logger.h>

static const char *MODULE_PREFIX = "RdWebRespREST";

// #define DEBUG_RESPONDER_REST_API
// #define DEBUG_MULTIPART_EVENTS
// #define DEBUG_MULTIPART_HEADERS
// #define DEBUG_MULTIPART_DATA

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebResponderRestAPI::RdWebResponderRestAPI(RestAPIEndpointDef* pEndpointDef, RdWebHandler* pWebHandler, 
                    const RdWebRequestParams& params, String& reqStr, const RdWebRequestHeaderExtract& headerExtract)
    : _reqParams(params)
{
    _pEndpointDef = pEndpointDef;
    _pWebHandler = pWebHandler;
    _endpointCalled = false;
    _requestStr = reqStr;
    _headerExtract = headerExtract;

    // Hook up callbacks
    _multipartParser.onEvent = std::bind(&RdWebResponderRestAPI::multipartOnEvent, this, 
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    _multipartParser.onData = std::bind(&RdWebResponderRestAPI::multipartOnData, this, 
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, 
            std::placeholders::_5);
    _multipartParser.onHeaderNameValue = std::bind(&RdWebResponderRestAPI::multipartOnHeaderNameValue, this, 
            std::placeholders::_1, std::placeholders::_2);

    // Check if multipart
    if (_headerExtract.isMultipart)
    {
        _multipartParser.setBoundary(_headerExtract.multipartBoundary);
    }

#ifdef DEBUG_RESPONDER_REST_API
    LOG_I(MODULE_PREFIX, "constr new responder %lx endpointDef %lx reqStr %s", (unsigned long)this, 
                    (unsigned long)pEndpointDef, reqStr.c_str());
#endif
}

RdWebResponderRestAPI::~RdWebResponderRestAPI()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle inbound data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderRestAPI::handleData(const uint8_t* pBuf, uint32_t dataLen)
{
    // Record data received so we know when to respond
    _numBytesReceived += dataLen;

    // Handle data which may be multipart
    if (_headerExtract.isMultipart)
    {
#ifdef DEBUG_RESPONDER_REST_API
        LOG_I(MODULE_PREFIX, "handleData multipart len %d", dataLen);
#endif
        _multipartParser.handleData(pBuf, dataLen);
#ifdef DEBUG_RESPONDER_REST_API
        LOG_I(MODULE_PREFIX, "handleData multipart finished bytesRx %d contentLen %d", 
                    _numBytesReceived, _headerExtract.contentLength);
#endif
    }
    else
    {
#ifdef DEBUG_RESPONDER_REST_API
        LOG_I(MODULE_PREFIX, "handleData len %d", dataLen);
#endif
        // Send as the body
        if (_pEndpointDef)
            _pEndpointDef->callbackBody(_requestStr, pBuf, dataLen, 0, dataLen);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Start responding
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderRestAPI::startResponding(RdWebConnection& request)
{
    _isActive = true;
    _endpointCalled = false;
    _numBytesReceived = 0;

#ifdef DEBUG_RESPONDER_REST_API
    LOG_I(MODULE_PREFIX, "startResponding isActive %d", _isActive);
#endif
    return _isActive;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get response next
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t RdWebResponderRestAPI::getResponseNext(uint8_t* pBuf, uint32_t bufMaxLen)
{
    // Check if all data received
    if (_numBytesReceived != _headerExtract.contentLength)
        return 0;

#ifdef DEBUG_RESPONDER_REST_API
    LOG_I(MODULE_PREFIX, "getResponseNext maxRespLen %d endpointCalled %d isActive %d", 
                    bufMaxLen, _endpointCalled, _isActive);
#endif

    // Check if we need to call API
    uint32_t respLen = 0;
    if (!_endpointCalled && _pEndpointDef)
    {
        // Call endpoint
        String retStr;
        _pEndpointDef->callback(_requestStr, retStr);

        // Check how much of buffer to send
        respLen = bufMaxLen > retStr.length() ? retStr.length() : bufMaxLen;
        if (respLen < retStr.length())
        {
            _respStr = retStr.substring(respLen);
            LOG_I(MODULE_PREFIX, "getResponseNext API response too long %d sending first part %d URL %s",
                        retStr.length(), respLen, _requestStr.c_str());
        }
        else
        {
            // Done response
            _isActive = false;
        }

        // Copy response
        memcpy(pBuf, retStr.c_str(), respLen);

        // Endpoint done
        _endpointCalled = true;
    }
    else if (_endpointCalled)
    {
        // Check how much of buffer to send
        respLen = bufMaxLen > _respStr.length() ? _respStr.length() : bufMaxLen;
        if (respLen < _respStr.length())
        {
            // Copy response
            memcpy(pBuf, _respStr.c_str(), respLen);

            // Remove sent data from _respStr
            _respStr = _respStr.substring(respLen);
            LOG_I(MODULE_PREFIX, "getResponseNext API next chunk len %d URL %s",
                        respLen, _requestStr.c_str());
        }
        else
        {
            // Copy response
            memcpy(pBuf, _respStr.c_str(), respLen);
            // Done response
            _isActive = false;
            _respStr.clear();
        }
    }
    else
    {
        // Done response
        _isActive = false;
    }

#ifdef DEBUG_RESPONDER_REST_API
    LOG_I(MODULE_PREFIX, "getResponseNext respLen %d isActive %d", respLen, _isActive);
#endif
    return respLen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get content type
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* RdWebResponderRestAPI::getContentType()
{
    return "text/json";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Leave connection open
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebResponderRestAPI::leaveConnOpen()
{
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks on multipart parser
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebResponderRestAPI::multipartOnEvent(RdMultipartEvent event, const uint8_t *pBuf, uint32_t pos)
{
#ifdef DEBUG_MULTIPART_EVENTS
    LOG_W(MODULE_PREFIX, "multipartEvent event %s (%d) pos %d", RdWebMultipart::getEventText(event), event, pos);
#endif
}

void RdWebResponderRestAPI::multipartOnData(const uint8_t *pBuf, uint32_t bufLen, RdMultipartForm& formInfo, 
                    uint32_t contentPos, bool isFinalPart)
{
#ifdef DEBUG_MULTIPART_DATA
    LOG_W(MODULE_PREFIX, "multipartData len %d filename %s contentPos %d isFinal %d", 
                bufLen, formInfo._fileName.c_str(), contentPos, isFinalPart);
#endif
    // Upload info
    FileBlockInfo fileBlockInfo(formInfo._fileName.c_str(), 
                    _headerExtract.contentLength, contentPos, 
                    pBuf, bufLen, isFinalPart, formInfo._crc16, formInfo._crc16Valid,
                    formInfo._fileLenBytes, formInfo._fileLenValid);
    // Check for callback
    if (_pEndpointDef)
        _pEndpointDef->callbackUpload(_requestStr, fileBlockInfo);
}

void RdWebResponderRestAPI::multipartOnHeaderNameValue(const String& name, const String& val)
{
#ifdef DEBUG_MULTIPART_HEADERS
    LOG_W(MODULE_PREFIX, "multipartHeaderNameValue %s = %s", name.c_str(), val.c_str());
#endif
}
