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
    void setNewConn(struct netconn *pConnection, RdWebConnManager* pConnManager);

    // True if active
    bool isActive();

    // Get header
    RdWebRequestHeader& getHeader()
    {
        return _header;
    }

    // Send buffer size
    static const uint32_t SEND_BUFFER_MAX_LEN = 5000;

private:
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
    bool serviceRxData(struct netbuf** pInbuf);

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
};
