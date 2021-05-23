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

#define WEB_CONN_USE_BERKELEY_SOCKETS

// Handle different platforms and netconn vs sockets
#ifndef ESP8266
#ifdef WEB_CONN_USE_BERKELEY_SOCKETS
typedef int RdWebConnClientType;
#define WEB_CONN_CLIENT_INVALID_VALUE (-1)
#else
typedef struct netconn* RdWebConnClientType;
#define WEB_CONN_CLIENT_INVALID_VALUE nullptr
#endif

#ifdef CONFIG_LWIP_TCP_MSS
#define WEB_CONN_MAX_RX_BUFFER CONFIG_LWIP_TCP_MSS
#else
#define WEB_CONN_MAX_RX_BUFFER 1440
#endif

extern "C"
{
#undef INADDR_NONE
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
}
#else // ESP8266
#include <WiFiClient.h>
#include <ESP8266Utils.h>
typedef WiFiClient* RdWebConnClientType;
#endif //ESP8266

// #define DEBUG_TRACE_HEAP_USAGE_WEB_CONN

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
    bool sendOnWebSocket(const uint8_t* pBuf, uint32_t bufLen);

    // Send on server-side events
    void sendOnSSEvents(const char* eventContent, const char* eventGroup);

    // Clear
    void clear();

    // Close and clear
    void closeAndClear();

    // Set a new connection
    void setNewConn(RdWebConnClientType& connClient, RdWebConnManager* pConnManager,
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
    RdWebConnClientType _connClient;

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
    RdHttpStatusCode _httpResponseStatus;

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

#ifndef WEB_CONN_USE_BERKELEY_SOCKETS
    // Service data
    bool getRxData(struct netbuf** pInbuf, bool& closeRequired);
#endif

    // Service connection header
    bool serviceConnHeader(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos);

    // Service responder
    bool serviceResponse(const uint8_t* pRxData, uint32_t dataLen, uint32_t& curBufPos);

    // Set HTTP response status
    void setHTTPResponseStatus(RdHttpStatusCode reponseCode);

    // Raw send on connection - used by websockets, etc
    bool rawSendOnConn(const uint8_t* pBuf, uint32_t bufLen);    

    // Send standard headers
    void sendStandardHeaders();

    // Handle response using buffer provided
    void handleResponseWithBuffer(uint8_t* pSendBuffer);
};
