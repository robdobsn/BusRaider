/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TCPConnection
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "TCPConnection.h"
#include "TCPConnManager.h"
#include <Logger.h>
#include <Utils.h>
#include <RdJson.h>
#include <functional>

static const char *MODULE_PREFIX = "TCPConn";

// #define DEBUG_TCP_CONN
// #define DEBUG_TCP_RX_START_END
// #define DEBUG_TCP_SOCKET_SEND

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TCPConnection::TCPConnection()
{
    // Clear
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TCPConnection::~TCPConnection()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set new connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnection::setNewConn(struct netconn *pConnection, RdWebConnManager* pConnManager)
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

    // Set connection timeout
    netconn_set_recvtimeout(pConnection, 1);

    // Debug
#ifdef DEBUG_TCP_CONN
    LOG_I(MODULE_PREFIX, "setNewConn pConn %lx", (unsigned long)_pConn);
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnection::clear()
{
    // Clear all fields
    _pConn = NULL;
    _pConnManager = NULL;
    _timeoutStartMs = 0;
    _timeoutDurationMs = 0;
    _timeoutActive = false;
    _header.clear();
}

void TCPConnection::closeAndClear()
{
#ifdef DEBUG_TCP_CONN
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

bool TCPConnection::isActive()
{
    return _pConn != NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send on socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnection::sendOnSocket(const uint8_t* pBuf, uint32_t bufLen)
{
#ifdef DEBUG_TCP_SOCKET_SEND
    LOG_I(MODULE_PREFIX, "sendOnSocket len %d pConn %lx", bufLen, (unsigned long)_pConn);
#endif

    // TODO 2020
    // rawSendOnConn

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service - called frequently
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPConnection::service()
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
    bool errorFound = false;
    if (!serviceRxData(&inbuf))
    {
        LOG_W(MODULE_PREFIX, "service error getting data pConn %lx", (unsigned long)_pConn);
        errorFound = true;
    }

    // Get any found data
    uint8_t *pBuf = NULL;
    uint16_t bufLen = 0;
    if (!errorFound && inbuf)
    {
        // Get a buffer
        err_t err = netbuf_data(inbuf, (void **)&pBuf, &bufLen);
        if ((err != ERR_OK) || !pBuf)
        {
            LOG_W(MODULE_PREFIX, "service netconn_data err %s buf NULL pConn %lx", netconnErrToStr(err), (unsigned long)_pConn);
            errorFound = true;
        }
    }

    // TODO 2020 - do something with the received data

    // Data all handled
    if (inbuf)
        netbuf_delete(inbuf);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service data reception
// False if connection closed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TCPConnection::serviceRxData(struct netbuf** pInbuf)
{
    // Debug
#ifdef DEBUG_TCP_RX_START_END
    LOG_W(MODULE_PREFIX, "service reading from client pConn %lx", (unsigned long)_pConn);
#endif

    // See if data available
    err_t err = netconn_recv(_pConn, pInbuf);

    // Check for timeout
    if (err == ERR_TIMEOUT)
    {
        // Debug
#ifdef DEBUG_TCP_RX_START_END
        LOG_W(MODULE_PREFIX, "service read nothing available pConn %lx", (unsigned long)_pConn);
#endif

        // Nothing to do
        return true;
    }

    // Check valid
    if (err != ERR_OK)
    {
        LOG_W(MODULE_PREFIX, "service netconn_recv error %s pConn %lx", netconnErrToStr(err), (unsigned long)_pConn);
        return false;
    }

    // Debug
#ifdef DEBUG_TCP_RX_START_END
    LOG_W(MODULE_PREFIX, "service has read from client OK pConn %lx", (unsigned long)_pConn);
#endif

    // Data available in pInbuf
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Raw send on connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TCPConnection::rawSendOnConn(const uint8_t* pBuf, uint32_t bufLen)
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
