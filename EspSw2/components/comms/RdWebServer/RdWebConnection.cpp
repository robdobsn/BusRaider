/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "RdWebConnection.h"
#include "RdWebConnDefs.h"
#include "RdWebHandler.h"
#include "RdWebConnManager.h"
#include "RdWebResponder.h"
#include <Logger.h>
#include <Utils.h>
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
// #define DEBUG_WEB_CONNECTION_DATA_PACKETS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RdWebConnection::RdWebConnection()
{
    // Responder
    _pResponder = NULL;
    
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
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set new connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::setNewConn(struct netconn *pConnection, RdWebConnManager* pConnManager,
                uint32_t maxSendBufferBytes)
{
    // Error check
    if (_pConn)
    {
        // Caller should only use this method if connection is inactive
        LOG_E(MODULE_PREFIX, "setNewConn existing connection active %lx", (unsigned long)_pConn);
        return;
    }

    // Clear first
    clear();

    // New connection
    _pConn = pConnection;
    _pConnManager = pConnManager;
    _timeoutStartMs = millis();
    _timeoutActive = true;
    _timeoutDurationMs = MAX_STD_CONN_DURATION_MS;
    _maxSendBufferBytes = maxSendBufferBytes;

    // Set connection timeout
    netconn_set_recvtimeout(pConnection, 1);

    // Debug
#ifdef DEBUG_WEB_CONN
    LOG_I(MODULE_PREFIX, "setNewConn pConn %lx", (unsigned long)_pConn);
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

    // Clear all fields
    _pResponder = NULL;
    _pConn = NULL;
    _pConnManager = NULL;
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
#ifdef DEBUG_WEB_CONN
    LOG_W(MODULE_PREFIX, "closeAndClear closing pConn %lx", (unsigned long)_pConn);
#endif

    // Close connection
    netconn_close(_pConn);
    netconn_delete(_pConn);

    // Clear
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check Active
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::isActive()
{
    return _pConn != NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send on websocket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::sendOnWebSocket(const uint8_t* pBuf, uint32_t bufLen)
{
#ifdef DEBUG_WEB_SOCKET_SEND
    LOG_I(MODULE_PREFIX, "sendOnWebSocket len %d responder %lx pConn %lx", bufLen, (unsigned long)_pResponder, 
                    (unsigned long)_pConn);
#endif

    // Send to responder
    if (_pResponder)
    {
        _pResponder->sendFrame(pBuf, bufLen);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::service()
{
    // Check active
    if (!_pConn)
        return;

    // Check timout
    if (_timeoutActive && Utils::isTimeout(millis(), _timeoutStartMs, _timeoutDurationMs))
    {
        LOG_W(MODULE_PREFIX, "service timeout on connection pConn %lx", (unsigned long)_pConn);
        closeAndClear();
        return;
    }

    // Check for data
    struct netbuf *inbuf = NULL;
    bool closeRequired = false;
    bool dataReady = getRxData(&inbuf, closeRequired);

    // Service responder
    if (_pResponder)
        _pResponder->service();

    // Get any found data
    uint8_t *pBuf = NULL;
    uint16_t bufLen = 0;
    bool errorOccurred = false;
    if (dataReady && inbuf)
    {
        // Get a buffer
        err_t err = netbuf_data(inbuf, (void **)&pBuf, &bufLen);
        if ((err != ERR_OK) || !pBuf)
        {
            LOG_W(MODULE_PREFIX, "service netconn_data err %s buf NULL pConn %lx", netconnErrToStr(err), (unsigned long)_pConn);
            errorOccurred = true;
        }
        else
        {
            _debugDataRxCount += bufLen;
#ifdef DEBUG_WEB_CONNECTION_DATA_PACKETS
            LOG_W(MODULE_PREFIX, "service netconn_data len %d rxTotal %d", bufLen, _debugDataRxCount);
#endif
        }
    }

    // See if we are forming the header
    uint32_t bufPos = 0;
    if (!errorOccurred && !_header.isComplete)
    {
        if (!serviceConnHeader(pBuf, bufLen, bufPos))
        {
            LOG_W(MODULE_PREFIX, "service connHeader error closing pConn %lx", (unsigned long)_pConn);
            errorOccurred = true;
        }
    }

    // Service response - may take many iterations of this loop (e.g. for file-transfer / web-sockets)

    if (!errorOccurred && _header.isComplete)
    {
        if (!serviceResponse(pBuf, bufLen, bufPos))
        {
#ifdef DEBUG_RESPONDER_PROGRESS
            LOG_I(MODULE_PREFIX, "service no longer sending so close pConn %lx", (unsigned long)_pConn);
#endif
            closeRequired = true;
        }
    }

    // Check for close
    if (errorOccurred || closeRequired)
    {
#ifdef DEBUG_WEB_CONN
        LOG_W(MODULE_PREFIX, "service conn closing cause %s pConn %lx", 
                errorOccurred ? "ErrorOccurred" : "CloseRequired", 
                (unsigned long)_pConn);
#endif
        closeAndClear();
    }

    // Data all handled
    if (inbuf)
        netbuf_delete(inbuf);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Rx data
// True if data is available
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RdWebConnection::getRxData(struct netbuf** pInbuf, bool& closeRequired)
{
    closeRequired = false;
    // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
    LOG_W(MODULE_PREFIX, "getRxData reading from client pConn %lx", (unsigned long)_pConn);
#endif

    // See if data available
    err_t err = netconn_recv(_pConn, pInbuf);

    // Check for timeout
    if (err == ERR_TIMEOUT)
    {
        // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
        LOG_W(MODULE_PREFIX, "getRxData read nothing available pConn %lx", (unsigned long)_pConn);
#endif

        // Nothing to do
        return false;
    }

    // Check for closed
    if (err == ERR_CLSD)
    {
        // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
        LOG_W(MODULE_PREFIX, "getRxData read connection closed pConn %lx", (unsigned long)_pConn);
#endif

        // Link is closed
        closeRequired = true;
        return false;
    }

    // Check for other error
    if (err != ERR_OK)
    {
#ifdef WARN_WEB_CONN_ERROR_CLOSE
        LOG_W(MODULE_PREFIX, "getRxData netconn_recv error %s pConn %lx", 
                    netconnErrToStr(err), (unsigned long)_pConn);
#endif
        closeRequired = true;
        return false;
    }

    // Debug
#ifdef DEBUG_WEB_REQUEST_READ_START_END
    LOG_W(MODULE_PREFIX, "getRxData has read from client OK pConn %lx", (unsigned long)_pConn);
#endif

    // Data available in pInbuf
    return true;
}

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
            getHTTPMethodStr(_header.extract.method), _header.URIAndParams.c_str(),
            _header.extract.contentType.c_str(), _header.extract.contentLength, _header.extract.host.c_str(), 
            _header.isContinue, _header.extract.isMultipart, _header.extract.multipartBoundary.c_str());
    LOG_I(MODULE_PREFIX, "onRxData headerExt auth %s isComplete %d isDigest %d reqConnType %d wsKey %s wsVers %s", 
            _header.extract.authorization.c_str(), _header.isComplete, _header.extract.isDigest, 
            _header.reqConnType, _header.webSocketKey.c_str(), _header.webSocketVersion.c_str());
#endif

    // Now find a responder
    HttpStatusCode statusCode = HTTP_STATUS_NOTFOUND;
        // Delete any existing responder - there shouldn't be one
        if (_pResponder)
        {
            LOG_W(MODULE_PREFIX, "onRxData unexpectedly deleting _pResponder %lx", (unsigned long)_pResponder);
            delete _pResponder;
            _pResponder = NULL;
        }

        // Get a responder (we are responsible for deletion)
    RdWebRequestParams params(_maxSendBufferBytes, _pConnManager->getStdResponseHeaders(), 
                    std::bind(&RdWebConnection::rawSendOnConn, this, std::placeholders::_1, std::placeholders::_2));
    _pResponder = _pConnManager->getNewResponder(_header, params, statusCode);
#ifdef DEBUG_RESPONDER_CREATE_DELETE
        if (_pResponder)
        LOG_I(MODULE_PREFIX, "New Responder created type %s", _pResponder->getResponderType());
    else
        LOG_W(MODULE_PREFIX, "Failed to create responder URI %s HTTP resp %d", _header.URIAndParams.c_str(), statusCode);
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
        if (_maxSendBufferBytes > MAX_BUFFER_ALLOCATED_ON_STACK)
        {
            uint8_t* pSendBuffer = new uint8_t[_maxSendBufferBytes];
            handleResponseWithBuffer(pSendBuffer);
            delete [] pSendBuffer;
            }
        else
        {
            uint8_t pSendBuffer[_maxSendBufferBytes];
            handleResponseWithBuffer(pSendBuffer);
        }
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
    LOG_I(MODULE_PREFIX, "serviceResponse atEnd responder %s isActive %s pConn %lx", 
                _pResponder ? "YES" : "NO", (_pResponder && _pResponder->isActive()) ? "YES" : "NO", (unsigned long)_pConn);
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
                    getHTTPMethodStr(_header.extract.method), _header.URL.c_str(), _header.params.c_str(),
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
            netconn_write(_pConn, response, strlen(response), NETCONN_COPY);
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
    for (int i = 0; i < WEB_REQ_METHODS_NUM; i++)
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

void RdWebConnection::setHTTPResponseStatus(HttpStatusCode responseCode)
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
    if (!_pConn)
        return false;

    // Send
    int err = netconn_write(_pConn, pBuf, bufLen, NETCONN_COPY);
    if (err != ERR_OK)
    {
        LOG_W(MODULE_PREFIX, "rawSendOnConn write failed err %s (%d) pConn %lx", netconnErrToStr(err), err, (unsigned long)_pConn);
    }
    return err == ERR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send standard headers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RdWebConnection::sendStandardHeaders()
{
    // Send the first line
    char respLine[100];
    snprintf(respLine, sizeof(respLine), "HTTP/1.1 %d %s\r\n", _httpResponseStatus, 
                getHTTPStatusStr(_httpResponseStatus));
    netconn_write(_pConn, respLine, strlen(respLine), NETCONN_COPY);
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
    // Debug
    LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %lx", respLine, (unsigned long) _pConn);
#endif

    // Get the content type
    if (_pResponder && _pResponder->getContentType())
    {
        snprintf(respLine, sizeof(respLine), "Content-Type: %s\r\n", _pResponder->getContentType());
        netconn_write(_pConn, respLine, strlen(respLine), NETCONN_COPY);
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
        // Debug
        LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %lx", respLine, (unsigned long) _pConn);
#endif
    }

    // Send other headers
    if (_pConnManager)
    {
        std::list<RdJson::NameValuePair>* pRespHeaders = _pConnManager->getStdResponseHeaders();
        for (RdJson::NameValuePair& nvPair : *pRespHeaders)
        {
            snprintf(respLine, sizeof(respLine), "%s: %s\r\n", nvPair.name.c_str(), nvPair.value.c_str());
            netconn_write(_pConn, respLine, strlen(respLine), NETCONN_COPY);
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
            // Debug
            LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %lx", respLine, (unsigned long) _pConn);
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
            netconn_write(_pConn, respLine, strlen(respLine), NETCONN_COPY);
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
            // Debug
            LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %lx", respLine, (unsigned long) _pConn);
#endif
        }
    }

    // Check if connection needs closing
    if (!_pResponder || !_pResponder->leaveConnOpen())
    {
        snprintf(respLine, sizeof(respLine), "Connection: close\r\n");
        netconn_write(_pConn, respLine, strlen(respLine), NETCONN_COPY);
#ifdef DEBUG_RESPONDER_HEADER_DETAIL
        // Debug
        LOG_I(MODULE_PREFIX, "serviceResponse sent %s pConn %lx", respLine, (unsigned long) _pConn);
#endif
    }

    // Send end of header line
    netconn_write(_pConn, "\r\n", 2, NETCONN_COPY);    
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
        LOG_I(MODULE_PREFIX, "serviceResponse writing %d pConn %lx", respSize, (unsigned long)_pConn);
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
        netconn_write(_pConn, pSendBuffer, respSize, NETCONN_COPY);
    }
}
