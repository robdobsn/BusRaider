/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RdWebServer
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
#include "RdWebConnDefs.h"
#include "RdWebRequestParams.h"
#include "RdWebRequestHeader.h"
extern "C"
{
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
#include "lwip/err.h"
}
class RdWebHandler;
class RdWebConnManager;
class RdWebResponder;

class RdWebConnection
{
public:
    RdWebConnection();
    virtual ~RdWebConnection();

    // Called frequently
    void service();

    // Send on web socket
    void sendOnWebSocket(const uint8_t* pBuf, uint32_t bufLen);

    // Clear
    void clear();

    // Close and clear
    void closeAndClear();

    // Set a new connection
    void setNewConn(struct netconn *pConnection, RdWebConnManager* pConnManager,
                uint32_t maxSendBufferBytes);

    // True if active
    bool isActive();

    // Get header
    RdWebRequestHeader& getHeader()
    {
        return _header;
    }

    // Get responder
    RdWebResponder* getResponder()
    {
        return _pResponder;
    }

private:

    // Max size of buffer permitted on the stack
    static const uint32_t MAX_BUFFER_ALLOCATED_ON_STACK = 1000;

    // Connection manager
    RdWebConnManager* _pConnManager;

    // Connection - we're active if this is not NULL
    struct netconn* _pConn;

    // Header parse info
    String _parseHeaderStr;

    // Header contents
    RdWebRequestHeader _header;

    // Responder
    RdWebResponder* _pResponder;

    // Send headers if needed
    bool _isStdHeaderRequired;
    bool _sendSpecificHeaders;

    // Response code if no responder available
    HttpStatusCode _httpResponseStatus;

    // Timeout timer
    static const uint32_t MAX_STD_CONN_DURATION_MS = 180000;
    uint32_t _timeoutStartMs;
    uint32_t _timeoutDurationMs;
    bool _timeoutActive;

    // Max send buffer size
    uint32_t _maxSendBufferBytes;

    // Debug
    uint32_t _debugDataRxCount;

    // Handle header data
    bool handleHeaderData(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos);

    // Parse a line of header section (including request line)
    // Returns false on failure
    bool parseHeaderLine(const String& line);

    // Parse the first line of HTTP request
    // Returns false on failure
    bool parseRequestLine(const String& line);

    // Parse name/value pairs in header line
    void parseNameValueLine(const String& reqLine);

    // Decode URL
    String decodeURL(const String &inURL) const;

    // Select handler
    void selectHandler();

    // Service data
    bool getRxData(struct netbuf** pInbuf, bool& closeRequired);

    // Service connection header
    bool serviceConnHeader(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos);

    // Service responder
    bool serviceResponse(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos);

    // Set HTTP response status
    void setHTTPResponseStatus(HttpStatusCode reponseCode);

    // Raw send on connection - used by websockets, etc
    bool rawSendOnConn(const uint8_t* pBuf, uint32_t bufLen);    

    // Send standard headers
    void sendStandardHeaders();

    // Handle response using buffer provided
    void handleResponseWithBuffer(uint8_t* pSendBuffer);
};
