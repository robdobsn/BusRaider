/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdWebConnection.h"
#include "RdWebInterface.h"
#include "RdWebHandler.h"
#include "RdWebConnManager.h"
#include "RdWebResponder.h"
#include <Logger.h>
#include <Utils.h>
#include <ArduinoTime.h>
#include <RdJson.h>
#include <functional>

static const char *MODULE_PREFIX = "RdWebConn";

#define WARN_WEB_CONN_ERROR_CLOSE
// #define DEBUG_WEB_CONN
// #define DEBUG_WEB_REQUEST_HEADERS
// #define DEBUG_WEB_REQUEST_HEADER_DETAIL
// #define DEBUG_WEB_REQUEST_READ
// #define DEBUG_WEB_REQUEST_RESP
// #define DEBUG_WEB_REQUEST_READ_START_END
// #define DEBUG_RESPONDER_PROGRESS
// #define DEBUG_RESPONDER_PROGRESS_DETAIL
// #define DEBUG_RESPONDER_HEADER_DETAIL
// #define DEBUG_RESPONDER_CONTENT_DETAIL
// #define DEBUG_RESPONDER_CREATE_DELETE
// #define DEBUG_WEB_SOCKET_SEND
// #define DEBUG_WEB_SSEVENT_SEND
// #define DEBUG_WEB_CONNECTION_DATA_PACKETS
// #define DEBUG_WEB_CONNECTION_DATA_PACKETS_CONTENTS
// #define DEBUG_WEB_CONN_OPEN_CLOSE
// #define DEBUG_WEB_CONN_SERVICE_TIME

#ifdef DEBUG_TRACE_HEAP_USAGE_WEB_CONN
#include "esp_heap_trace.h"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebConnection::RdWebConnection()
{
    // Responder
    _pResponder = nullptr;
    _connClient = WEB_CONN_CLIENT_INVALID_VALUE;
    
    // Clear
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebConnection::~RdWebConnection()
{
    // Check if there is a responder to clean up
    if (_pResponder)
    {
#ifdef DEBUG_RESPONDER_CREATE_DELETE
        LOG_W(MODULE_PREFIX, "destructor deleting _pResponder %lx", (unsigned long)_pResponder);
#endif
        delete _pResponder;
    }

#ifdef ESP8266
    if (_connClient)
    {
        _connClient->stop();
        delete _connClient;
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set new connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::setNewConn(RdWebConnClientType& connClient, RdWebConnManager* pConnManager,
                uint32_t maxSendBufferBytes)
{
    // Error check
    if (_connClient != WEB_CONN_CLIENT_INVALID_VALUE)
    {
        // Caller should only use this method if connection is inactive
        LOG_E(MODULE_PREFIX, "setNewConn existing connection active %ld", (unsigned long)_connClient);
        return;
    }

    // Clear first
    clear();

    // New connection
    _connClient = connClient;
    _pConnManager = pConnManager;
    _timeoutStartMs = millis();
    _timeoutActive = true;
    _timeoutDurationMs = MAX_STD_CONN_DURATION_MS;
    _maxSendBufferBytes = maxSendBufferBytes;

#ifndef ESP8266
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
    // Set non-blocking socket
    int flags = fcntl(_connClient, F_GETFL, 0);
    if (flags != -1)
       flags = flags | O_NONBLOCK;
   fcntl(_connClient, F_SETFL, flags);
#else
    // Set connection timeout
    netconn_set_recvtimeout(_connClient, 1);
#endif
#endif

    // Debug
#ifdef DEBUG_WEB_CONN_OPEN_CLOSE
    LOG_I(MODULE_PREFIX, "setNewConn conn %ld", (unsigned long)_connClient);
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::clear()
{
    // Delete responder if there is one
    if (_pResponder)
    {
#ifdef DEBUG_RESPONDER_CREATE_DELETE
        LOG_W(MODULE_PREFIX, "clear deleting _pResponder %lx", (unsigned long)_pResponder);
#endif
        delete _pResponder;
    }

#ifdef ESP8266
    if (_pConn)
    {
        delete _connClient;
    }
#endif

    // Clear all fields
    _pResponder = nullptr;
    _connClient = WEB_CONN_CLIENT_INVALID_VALUE;
    _pConnManager = nullptr;
    _isStdHeaderRequired = true;
    _sendSpecificHeaders = true;
    _httpResponseStatus = HTTP_STATUS_OK;
    _timeoutStartMs = 0;
    _timeoutDurationMs = 0;
    _timeoutActive = false;
    _parseHeaderStr = "";
    _debugDataRxCount = 0;
    _maxSendBufferBytes = 0;
    _header.clear();
}

void RdWebConnection::closeAndClear()
{
#ifdef DEBUG_WEB_CONN_OPEN_CLOSE
    LOG_I(MODULE_PREFIX, "closeAndClear closing conn %ld", (unsigned long)_connClient);
#endif

#ifndef ESP8266
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
    close(_connClient);
#else
    // Close connection
    netconn_close(_connClient);
    netconn_delete(_connClient);
#endif
#else
    if (_connClient)
        _connClient->stop();
#endif
    // Clear
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check Active
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::isActive()
{
    return _connClient != WEB_CONN_CLIENT_INVALID_VALUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send on websocket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::sendOnWebSocket(const uint8_t* pBuf, uint32_t bufLen)
{
#ifdef DEBUG_WEB_SOCKET_SEND
    LOG_I(MODULE_PREFIX, "sendOnWebSocket len %d responder %ld conn %ld", bufLen, (unsigned long)_pResponder, 
                    (unsigned long)_connClient);
#endif

    // Send to responder
    if (_pResponder)
        return _pResponder->sendFrame(pBuf, bufLen);

    // Failure
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send on websocket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::sendOnSSEvents(const char* eventContent, const char* eventGroup)
{
#ifdef DEBUG_WEB_SSEVENT_SEND
    LOG_I(MODULE_PREFIX, "sendOnSSEvents eventGroup %s eventContent %s responder %lx pConn %lx", 
                eventGroup, eventContent,
                (unsigned long)_pResponder, 
                (unsigned long)_pConn);
#endif

    // Send to responder
    if (_pResponder)
    {
        _pResponder->sendEvent(eventContent, eventGroup);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::service()
{
#ifdef DEBUG_WEB_CONN_SERVICE_TIME
    uint32_t debugServiceStartMs = millis();
#endif

    // Check active
    if (_connClient == WEB_CONN_CLIENT_INVALID_VALUE)
        return;

    // Check timout
    if (_timeoutActive && Utils::isTimeout(millis(), _timeoutStartMs, _timeoutDurationMs))
    {
        LOG_W(MODULE_PREFIX, "service timeout on connection conn %ld", (unsigned long)_connClient);
        closeAndClear();
        return;
    }

    // Service responder
    if (_pResponder)
        _pResponder->service();

    // Check if ready for data
    bool readyForData = (!_header.isComplete) || (_pResponder && _pResponder->readyForData());
    if (!readyForData)
        return;

    // Get data
    bool closeRequired = false;
    bool errorOccurred = false;

#ifndef ESP8266
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
    uint8_t* pBuf = new uint8_t[WEB_CONN_MAX_RX_BUFFER];
    if (!pBuf)
    {
        LOG_E(MODULE_PREFIX, "service failed to alloc %ld", (unsigned long)_connClient);
        return;
    }
    int32_t bufLen = recv(_connClient, pBuf, WEB_CONN_MAX_RX_BUFFER, 0);
    if (bufLen < 0)
    {
        switch(errno)
        {
            case EWOULDBLOCK:
                bufLen = 0;
                break;
            default:
                LOG_W(MODULE_PREFIX, "service read error %d", errno);
                errorOccurred = true;
                break;
        }
    }
    else if (bufLen == 0)
    {
        LOG_W(MODULE_PREFIX, "service read conn closed %d", errno);
        closeRequired = true;
    }
    else
    {
        _debugDataRxCount += bufLen;
#ifdef DEBUG_WEB_CONNECTION_DATA_PACKETS
        LOG_W(MODULE_PREFIX, "service read len %d rxTotal %d", bufLen, _debugDataRxCount);
#endif
#ifdef DEBUG_WEB_CONNECTION_DATA_PACKETS_CONTENTS
        String debugStr;
        Utils::getHexStrFromBytes(pBuf, bufLen, debugStr);
        LOG_I(MODULE_PREFIX, "RX: %s", debugStr.c_str());
#endif
    }
#else // WEB_CONN_USE_BERKELEY_SOCKETS
    // Check for data
    struct netbuf *inbuf = nullptr;
    bool dataReady = getRxData(&inbuf, closeRequired);

    // Get any found data
    uint8_t *pBuf = nullptr;
    uint16_t bufLen = 0;
    if (dataReady && inbuf)
    {
        // Get a buffer
        err_t err = netbuf_data(inbuf, (void **)&pBuf, &bufLen);
        if ((err != ERR_OK) || !pBuf)
        {
            LOG_W(MODULE_PREFIX, "service netconn_data err %s buf nullptr pConn %ld", 
                        RdWebInterface::espIdfErrToStr(err), (unsigned long)_connClient);
            errorOccurred = true;
        }
        else
        {
            _debugDataRxCount += bufLen;
#ifdef DEBUG_WEB_CONNECTION_DATA_PACKETS
            LOG_W(MODULE_PREFIX, "service netconn_data len %d rxTotal %d", bufLen, _debugDataRxCount);
#endif
#ifdef DEBUG_WEB_CONNECTION_DATA_PACKETS_CONTENTS
        String debugStr;
        Utils::getHexStrFromBytes(pBuf, bufLen, debugStr);
        LOG_I(MODULE_PREFIX, "RX: %s", debugStr.c_str());
#endif
        }
    }
#endif // WEB_CONN_USE_BERKELEY_SOCKETS 
#else // ESP8266
    // Check for data
    static const uint32_t MAX_BYTES_TO_RX = 1000;
    uint8_t dataBuf[MAX_BYTES_TO_RX];

    if (_pConn)
    {
        if (_pConn->connected())
        {
            while (_pConn->available()) 
            {
                bufLen = _pConn->read(dataBuf, MAX_BYTES_TO_RX);
            }
        }
        pBuf = dataBuf;
    }

    // Service responder
    if (_pResponder)
        _pResponder->service();

#endif // ESP8266

    // See if we are forming the header
    uint32_t bufPos = 0;
    if ((bufLen > 0) && !errorOccurred && !_header.isComplete)
    {
        if (!serviceConnHeader(pBuf, bufLen, bufPos))
        {
            LOG_W(MODULE_PREFIX, "service connHeader error closing conn %ld", 
                                        (unsigned long)_connClient);
            errorOccurred = true;
        }
    }

    // Service response - may remain in this state (e.g. for file-transfer / web-sockets)
    if ((bufLen > 0) && !errorOccurred && _header.isComplete)
    {
        if (!serviceResponse(pBuf, bufLen, bufPos))
        {
#ifdef DEBUG_RESPONDER_PROGRESS
            LOG_I(MODULE_PREFIX, "service no longer sending so close conn %ld", 
                            (unsigned long)_connClient);
#endif
            closeRequired = true;
        }
    }

    // Check for close
    if (errorOccurred || closeRequired)
    {
#ifdef DEBUG_WEB_CONN_OPEN_CLOSE
        LOG_I(MODULE_PREFIX, "service conn closing cause %s pConn %ld", 
                errorOccurred ? "ErrorOccurred" : "CloseRequired", 
                (unsigned long)_connClient);
#endif
        closeAndClear();
#ifdef DEBUG_TRACE_HEAP_USAGE_WEB_CONN
        heap_trace_stop();
        heap_trace_dump();
#endif
    }

#ifndef ESP8266
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
    // Delete buffer
    if (pBuf)
        delete pBuf;
#else
    // Data all handled
    if (inbuf)
        netbuf_delete(inbuf);
#endif
#endif

#ifdef DEBUG_WEB_CONN_SERVICE_TIME
    LOG_I(MODULE_PREFIX, "service elapsed %ld", millis() - debugServiceStartMs);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Rx data
// True if data is available
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ESP8266
#ifndef WEB_CONN_USE_BERKELEY_SOCKETS

bool RdWebConnection::getRxData(struct netbuf** pInbuf, bool& closeRequired)
{
    closeRequired = false;
    // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
    LOG_W(MODULE_PREFIX, "getRxData reading from client pConn %ld", 
                (unsigned long)_connClient);
#endif

    // See if data available
    err_t err = netconn_recv(_connClient, pInbuf);

    // Check for timeout
    if (err == ERR_TIMEOUT)
    {
        // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
        LOG_W(MODULE_PREFIX, "getRxData read nothing available pConn %ld", 
                        (unsigned long)_connClient);
#endif

        // Nothing to do
        return false;
    }

    // Check for closed
    if (err == ERR_CLSD)
    {
        // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
        LOG_W(MODULE_PREFIX, "getRxData read connection closed pConn %ld", 
                        (unsigned long)_connClient);
#endif

        // Link is closed
        closeRequired = true;
        return false;
    }

    // Check for other error
    if (err != ERR_OK)
    {
#ifdef WARN_WEB_CONN_ERROR_CLOSE
        LOG_W(MODULE_PREFIX, "getRxData netconn_recv error %s pConn %ld", 
                    RdWebInterface::espIdfErrToStr(err), (unsigned long)_connClient);
#endif
        closeRequired = true;
        return false;
    }

    // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
    LOG_W(MODULE_PREFIX, "getRxData has read from client OK pConn %ld", 
                (unsigned long)_connClient);
#endif

    // Data available in pInbuf
    return true;
}
#endif // WEB_CONN_USE_BERKELEY_SOCKETS
#endif // ESP8266

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service connection header
// Returns false on header error
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::serviceConnHeader(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos)
{
    if (!_pConnManager)
        return false;

    // Check for data
    if (dataLen == 0)
        return true;

    // Debug
#ifdef DEBUG_WEB_REQUEST_HEADER_DETAIL
    {
        String rxStr;
        Utils::strFromBuffer(pRxData, dataLen, rxStr);
        LOG_I(MODULE_PREFIX, "req data:\n%s", rxStr.c_str());
    }
#endif

    // Handle data for header
    bool headerOk = handleHeaderData(pRxData, dataLen, curBufPos);
    if (!headerOk)
    {
        setHTTPResponseStatus(HTTP_STATUS_BADREQUEST);
        return false;
    }

    // Check if header if now complete
    if (!_header.isComplete)
        return true;

    // Debug
#ifdef DEBUG_WEB_REQUEST_HEADERS
    LOG_I(MODULE_PREFIX, "onRxData headersOK method %s fullURI %s contentType %s contentLength %d host %s isContinue %d isMutilpart %d multipartBoundary %s", 
            RdWebInterface::getHTTPMethodStr(_header.extract.method), _header.URIAndParams.c_str(),
            _header.extract.contentType.c_str(), _header.extract.contentLength, _header.extract.host.c_str(), 
            _header.isContinue, _header.extract.isMultipart, _header.extract.multipartBoundary.c_str());
    LOG_I(MODULE_PREFIX, "onRxData headerExt auth %s isComplete %d isDigest %d reqConnType %d wsKey %s wsVers %s", 
            _header.extract.authorization.c_str(), _header.isComplete, _header.extract.isDigest, 
            _header.reqConnType, _header.webSocketKey.c_str(), _header.webSocketVersion.c_str());
#endif

    // Now find a responder
    RdHttpStatusCode statusCode = HTTP_STATUS_NOTFOUND;
    // Delete any existing responder - there shouldn't be one
    if (_pResponder)
    {
        LOG_W(MODULE_PREFIX, "onRxData unexpectedly deleting _pResponder %ld", (unsigned long)_pResponder);
        delete _pResponder;
        _pResponder = nullptr;
    }

    // Get a responder (we are responsible for deletion)
    RdWebRequestParams params(_maxSendBufferBytes, _pConnManager->getStdResponseHeaders(), 
                std::bind(&RdWebConnection::rawSendOnConn, this, std::placeholders::_1, std::placeholders::_2));
    _pResponder = _pConnManager->getNewResponder(_header, params, statusCode);
#ifdef DEBUG_RESPONDER_CREATE_DELETE
    if (_pResponder) {
        LOG_I(MODULE_PREFIX, "New Responder created type %s", _pResponder->getResponderType());
    } else {
        LOG_W(MODULE_PREFIX, "Failed to create responder URI %s HTTP resp %d", _header.URIAndParams.c_str(), statusCode);
    }
#endif

    // Check we got a responder
    if (!_pResponder)
    {
        setHTTPResponseStatus(statusCode);
    }
    else
    {
        // Remove timeouts on long-running responders
        if (_pResponder->leaveConnOpen())
            _timeoutActive = false;

        // Start responder
        _pResponder->startResponding(*this);
    }

    // Ok
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service responder
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::serviceResponse(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos)
{
    // Hand any data to responder (if there is one)
    if (_pResponder && (curBufPos < dataLen))
        _pResponder->handleData(pRxData+curBufPos, dataLen-curBufPos);

    // Service the responder (if there is one)
    if (_pResponder)
        _pResponder->service();

    // Handle active responder responses
    if (_pResponder && _pResponder->isActive())
    {
        // Get next chunk of response
#ifndef ESP8266
        if (_maxSendBufferBytes > MAX_BUFFER_ALLOCATED_ON_STACK)
        {
#endif
            uint8_t* pSendBuffer = new uint8_t[_maxSendBufferBytes];
            handleResponseWithBuffer(pSendBuffer);
            delete [] pSendBuffer;
#ifndef ESP8266
        }
        else
        {
            uint8_t pSendBuffer[_maxSendBufferBytes];
            handleResponseWithBuffer(pSendBuffer);
        }
#endif
    }

    // Send the standard response and headers if required    
    else if (_isStdHeaderRequired && (!_pResponder || _pResponder->isStdHeaderRequired()))
    {
        // Send standard headers
        sendStandardHeaders();

        // Done headers
        _isStdHeaderRequired = false;
    }

    // Debug
#ifdef DEBUG_RESPONDER_PROGRESS_DETAIL
    LOG_I(MODULE_PREFIX, "serviceResponse atEnd responder %s isActive %s pConn %ld", 
                _pResponder ? "YES" : "NO", 
                (_pResponder && _pResponder->isActive()) ? "YES" : "NO", 
                (unsigned long)_connClient);
#endif

    // If no responder then that's it
    if (!_pResponder)
        return false;

    // Return indication of more to come
    return _pResponder->isActive();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle header data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::handleHeaderData(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos)
{
    // Go through received data extracting headers
    uint32_t pos = 0;
    while (true)
    {
        // Find eol if there is one
        uint32_t lfFoundPos = 0;
        for (lfFoundPos = pos; lfFoundPos < dataLen; lfFoundPos++)
            if (pRxData[lfFoundPos] == '\n')
                break;

        // Extract string
        String newStr;
        Utils::strFromBuffer(pRxData + pos, lfFoundPos - pos, newStr);
        newStr.trim();

        // Add to parse header string
        _parseHeaderStr += newStr;

        // Move on
        pos = lfFoundPos + 1;

        // Check if we have found a full line
        if (lfFoundPos != dataLen)
        {
            // Parse header line
            if (!parseHeaderLine(_parseHeaderStr))
                return false;
            _parseHeaderStr = "";
        }

        // Check all done
        if ((pos >= dataLen) || (_header.isComplete))
            break;
    }
    curBufPos = pos;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse header line
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::parseHeaderLine(const String& line)
{
    // Headers
#ifdef DEBUG_WEB_REQUEST_HEADER_DETAIL
    LOG_I(MODULE_PREFIX, "header line len %d = %s", line.length(), line.c_str());
#endif

    // Check if we're looking at the request line
    if (!_header.gotFirstLine)
    {
        // Check blank request line
        if (line.length() == 0)
            return false;

        // Parse method, etc
        if (!parseRequestLine(line))
            return false;

        // Debug
#ifdef DEBUG_WEB_REQUEST_HEADERS
        LOG_I(MODULE_PREFIX, "parseHeaderLine method %s URL %s params %s fullURI %s", 
                    RdWebInterface::getHTTPMethodStr(_header.extract.method), _header.URL.c_str(), _header.params.c_str(),
                    _header.URIAndParams.c_str());
#endif

        // Next parsing headers
        _header.gotFirstLine = true;
        return true;
    }

    // Check if we've finished all lines
    if (line.length() == 0)
    {
        // Debug
#ifdef DEBUG_WEB_REQUEST_HEADERS
        LOG_I(MODULE_PREFIX, "End of headers");
#endif

        // Check if continue required
        if (_header.isContinue)
        {
            const char *response = "HTTP/1.1 100 Continue\r\n\r\n";
            rawSendOnConn((const uint8_t*) response, strlen(response));
        }

        // Header now complete
        _header.isComplete = true;
    }
    else
    {
        // Handle each line of header
        parseNameValueLine(line);
    }

    // Ok
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse first line of HTTP header
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::parseRequestLine(const String& reqLine)
{
    // Methods
    static const char* WEB_REQ_METHODS [] = { "GET", "POST", "DELETE", "PUT", "PATCH", "HEAD", "OPTIONS" };
    static const RdWebServerMethod WEB_REQ_METHODS_ENUM [] = { WEB_METHOD_GET, WEB_METHOD_POST, WEB_METHOD_DELETE, 
                        WEB_METHOD_PUT, WEB_METHOD_PATCH, WEB_METHOD_HEAD, WEB_METHOD_OPTIONS };
    static const uint32_t WEB_REQ_METHODS_NUM = sizeof(WEB_REQ_METHODS) / sizeof(WEB_REQ_METHODS[0]);

    // Method
    int sepPos = reqLine.indexOf(' ');
    String method = reqLine.substring(0, sepPos);
    _header.extract.method = WEB_METHOD_NONE;
    for (uint32_t i = 0; i < WEB_REQ_METHODS_NUM; i++)
    {
        if (method.equalsIgnoreCase(WEB_REQ_METHODS[i]))
        {
            _header.extract.method = WEB_REQ_METHODS_ENUM[i];
            break;
        }
    }

    // Check valid
    if (_header.extract.method == WEB_METHOD_NONE)
        return false;

    // URI
    int sep2Pos = reqLine.indexOf(' ', sepPos+1);
    if (sep2Pos < 0)
        return false;
    _header.URIAndParams = decodeURL(reqLine.substring(sepPos+1, sep2Pos));

    // Split out params if present
    _header.URL = _header.URIAndParams;
    int paramPos = _header.URIAndParams.indexOf('?');
    _header.params = "";
    if (paramPos > 0)
    {
        _header.URL = _header.URIAndParams.substring(0, paramPos);
        _header.params = _header.URIAndParams.substring(paramPos+1);
    }

    // Remainder is the version string
    _header.versStr = reqLine.substring(sep2Pos+1);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse name/value pairs of HTTP header
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::parseNameValueLine(const String& reqLine)
{
    // Extract header name/value pairs
    int nameEndPos = reqLine.indexOf(':');
    if (nameEndPos < 0)
        return;

    // Parts
    String name = reqLine.substring(0, nameEndPos);
    String val = reqLine.substring(nameEndPos+2);

    // Store
    if (_header.nameValues.size() >= RdWebRequestHeader::MAX_WEB_HEADERS)
        return;
    _header.nameValues.push_back({name, val});

    // Handle named headers
    // Parsing derived from AsyncWebServer menodev
    if (name.equalsIgnoreCase("Host"))
    {
        _header.extract.host = val;
    }
    else if (name.equalsIgnoreCase("Content-Type"))
    {
        _header.extract.contentType = val.substring(0, val.indexOf(';'));
        if (val.startsWith("multipart/"))
        {
            _header.extract.multipartBoundary = val.substring(val.indexOf('=') + 1);
            _header.extract.multipartBoundary.replace("\"", "");
            _header.extract.isMultipart = true;
        }
    }
    else if (name.equalsIgnoreCase("Content-Length"))
    {
        _header.extract.contentLength = atoi(val.c_str());
    }
    else if (name.equalsIgnoreCase("Expect") && val.equalsIgnoreCase("100-continue"))
    {
        _header.isContinue = true;
    }
    else if (name.equalsIgnoreCase("Authorization"))
    {
        if (val.length() > 5 && val.substring(0, 5).equalsIgnoreCase("Basic"))
        {
            _header.extract.authorization = val.substring(6);
        }
        else if (val.length() > 6 && val.substring(0, 6).equalsIgnoreCase("Digest"))
        {
            _header.extract.isDigest = true;
            _header.extract.authorization = val.substring(7);
        }
    }
    else if (name.equalsIgnoreCase("Upgrade") && val.equalsIgnoreCase("websocket"))
    {
        // WebSocket request can be uniquely identified by header: [Upgrade: websocket]
        _header.reqConnType = REQ_CONN_TYPE_WEBSOCKET;
    }
    else if (name.equalsIgnoreCase("Accept"))
    {
        String acceptLC = val;
        acceptLC.toLowerCase();
        if (acceptLC.indexOf("text/event-stream") >= 0)
        {
            // WebEvent request can be uniquely identified by header:  [Accept: text/event-stream]
            _header.reqConnType = REQ_CONN_TYPE_EVENT;
        }
    }
    else if (name.equalsIgnoreCase("Sec-WebSocket-Key"))
    {
        _header.webSocketKey = val;
    }
    else if (name.equalsIgnoreCase("Sec-WebSocket-Version"))
    {
        _header.webSocketVersion = val;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Decode URL escaped string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String RdWebConnection::decodeURL(const String &inURL) const
{
    // Go through handling encoding
    const char* pCh = inURL.c_str();
    String outURL;
    outURL.reserve(inURL.length());
    while (*pCh)
    {
        // Check for % escaping
        if ((*pCh == '%') && *(pCh+1) && *(pCh+2))
        {
            char newCh = Utils::getHexFromChar(*(pCh+1)) * 16 + Utils::getHexFromChar(*(pCh+2));
            outURL.concat(newCh);
            pCh += 3;
        }
        else
        {
            outURL.concat(*pCh == '+' ? ' ' : *pCh);
            pCh++;
        }
    }
    return outURL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set HTTP response status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::setHTTPResponseStatus(RdHttpStatusCode responseCode)
{
#ifdef DEBUG_WEB_REQUEST_RESP
    LOG_I(MODULE_PREFIX, "Setting response code %s (%d)", getHTTPStatusStr(responseCode), responseCode);
#endif
    _httpResponseStatus = responseCode;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Raw send on connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::rawSendOnConn(const uint8_t* pBuf, uint32_t bufLen)
{
    // Check active
    if (_connClient == WEB_CONN_CLIENT_INVALID_VALUE)
    {
        LOG_W(MODULE_PREFIX, "rawSendOnConn invalid conn %ld", 
                    (unsigned long) _connClient);
        return false;
    }

    // Send
#ifndef ESP8266
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
    int rslt = send(_connClient, pBuf, bufLen, 0);
    if (rslt < 0)
    {
        LOG_W(MODULE_PREFIX, "rawSendOnConn write failed errno %d conn %ld", 
                    errno, (unsigned long) _connClient);
        return false;
    }
#ifdef DEBUG_WEB_CONNECTION_DATA_PACKETS_CONTENTS
    String debugStr;
    Utils::getHexStrFromBytes(pBuf, bufLen, debugStr);
    LOG_I(MODULE_PREFIX, "conn %ld TX: %s", (unsigned long)_connClient, debugStr.c_str());
#endif
    return true;
#else
    int err = netconn_write(_connClient, pBuf, bufLen, NETCONN_COPY);
    if (err != ERR_OK)
    {
        LOG_W(MODULE_PREFIX, "rawSendOnConn write failed err %s (%d) pConn %ld", 
                    RdWebInterface::espIdfErrToStr(err), err, (unsigned long)_connClient);
    }
#ifdef DEBUG_WEB_CONNECTION_DATA_PACKETS_CONTENTS
    String debugStr;
    Utils::getHexStrFromBytes(pBuf, bufLen, debugStr);
    LOG_I(MODULE_PREFIX, "conn %ld TX: %s", (unsigned long)_connClient, debugStr.c_str());
#endif
    return err == ERR_OK;
#endif
#else // ESP8266
    int err = ERR_OK;
    size_t written = _connClient->write((const char*)pBuf, bufLen);
    if (written != bufLen)
    {
        LOG_I(MODULE_PREFIX, "connectionWrite written %d != size %d", written, size);
        err = ERR_CONN;
    }
    return err = ERR_OK;
#endif // ESP8266
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send standard headers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::sendStandardHeaders()
{
    // Send the first line
    char respLine[100];
    snprintf(respLine, sizeof(respLine), "HTTP/1.1 %d %s\r\n", _httpResponseStatus, 
                RdWebInterface::getHTTPStatusStr(_httpResponseStatus));
    rawSendOnConn((const uint8_t*)respLine, strlen(respLine));
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
    // Debug
    LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %ld", 
                    respLine, (unsigned long) _connClient);
#endif

    // Get the content type
    if (_pResponder && _pResponder->getContentType())
    {
        snprintf(respLine, sizeof(respLine), "Content-Type: %s\r\n", _pResponder->getContentType());
        rawSendOnConn((const uint8_t*)respLine, strlen(respLine));
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
        // Debug
        LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %ld", 
                        respLine, (unsigned long) _connClient);
#endif
    }

    // Send other headers
    if (_pConnManager)
    {
        std::list<RdJson::NameValuePair>* pRespHeaders = _pConnManager->getStdResponseHeaders();
        for (RdJson::NameValuePair& nvPair : *pRespHeaders)
        {
            snprintf(respLine, sizeof(respLine), "%s: %s\r\n", nvPair.name.c_str(), nvPair.value.c_str());
            rawSendOnConn((const uint8_t*)respLine, strlen(respLine));
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
            // Debug
            LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %ld", 
                        respLine, (unsigned long) _connClient);
#endif
        }
    }

    // Content length if required
    if (_pResponder)
    {
        int contentLength = _pResponder->getContentLength();
        if (contentLength >= 0)
        {
            snprintf(respLine, sizeof(respLine), "Content-Length: %d\r\n", contentLength);
            rawSendOnConn((const uint8_t*)respLine, strlen(respLine));
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
            // Debug
            LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %ld", 
                        respLine, (unsigned long) _connClient);
#endif
        }
    }

    // Check if connection needs closing
    if (!_pResponder || !_pResponder->leaveConnOpen())
    {
        snprintf(respLine, sizeof(respLine), "Connection: close\r\n");
        rawSendOnConn((const uint8_t*)respLine, strlen(respLine));
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
        // Debug
        LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %ld", 
                respLine, (unsigned long) _connClient);
#endif
    }

    // Send end of header line
    rawSendOnConn((const uint8_t*)"\r\n", 2);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle response with buffer provided
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::handleResponseWithBuffer(uint8_t* pSendBuffer)
{
    uint32_t respSize = _pResponder->getResponseNext(pSendBuffer, _maxSendBufferBytes);

    // Check valid
    if (respSize != 0)
    {
        // Debug
#ifdef DEBUG_RESPONDER_CONTENT_DETAIL
        LOG_I(MODULE_PREFIX, "serviceResponse writing %d conn %ld", 
                        respSize, (unsigned long)_connClient);
#endif

        // Check if standard reponse to be sent first
        if (_isStdHeaderRequired && _pResponder->isStdHeaderRequired())
        {
            // Send standard headers
            sendStandardHeaders();

            // Done headers
            _isStdHeaderRequired = false;
        }

        // Send
        rawSendOnConn((const uint8_t*)pSendBuffer, respSize);
    }
}
