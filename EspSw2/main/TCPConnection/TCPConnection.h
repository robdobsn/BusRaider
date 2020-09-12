/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TCPConnection
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <WString.h>
extern "C"
{
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
#include "lwip/err.h"
}

class TCPConnManager;

class TCPConnection
{
public:
    TCPConnection();
    virtual ~TCPConnection();

    // Called frequently
    void service();

    // Send on socket
    void sendOnSocket(const uint8_t* pBuf, uint32_t bufLen);

    // Clear
    void clear();

    // Close and clear
    void closeAndClear();

    // Set a new connection
    void setNewConn(struct netconn *pConnection, RdWebConnManager* pConnManager);

    // True if active
    bool isActive();

    // Send buffer size
    static const uint32_t SEND_BUFFER_MAX_LEN = 5000;

    // TCP connection types
    enum ConnType
    {
        CONN_TYPE_NONE = -1,
        CONN_TYPE_PLAIN_TCP,
        CONN_TYPE_MAX
    };

    // Get conn type string
    static const char* getConnTypeStr(ConnType connType)
    {
        switch(connType)
        {
            default:
            case CONN_TYPE_NONE: return "NONE";
            case CONN_TYPE_PLAIN_TCP: return "PLAINTCP"; 
        }
        return "NONE";
    }

private:
    // Connection manager
    TCPConnManager* _pConnManager;

    // Connection - we're active if this is not NULL
    struct netconn* _pConn;

    // Timeout timer
    static const uint32_t MAX_STD_CONN_DURATION_MS = 30000;
    uint32_t _timeoutStartMs;
    uint32_t _timeoutDurationMs;
    bool _timeoutActive;

    // Service data
    bool serviceRxData(struct netbuf** pInbuf);

    // Raw send on connection
    bool rawSendOnConn(const uint8_t* pBuf, uint32_t bufLen);    
};
