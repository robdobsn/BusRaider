/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandSocket
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include <CommandSocket.h>
#include <Utils.h>
#include <RestAPIEndpointManager.h>
#include "ProtocolEndpointManager.h"
#include <NetworkSystem.h>

static const char *MODULE_PREFIX = "CommandSocket";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommandSocket::CommandSocket(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Config variables
    _isEnabled = false;
    _port = 0;
    _begun = false;

#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
    _pTcpServer = NULL;
    _clientMutex = xSemaphoreCreateMutex();
#endif

    // EndpointID
    _protocolEndpointID = ProtocolEndpointManager::UNDEFINED_ID;
}

CommandSocket::~CommandSocket()
{
#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
    if (_pTcpServer)
    {
        end();
        delete _pTcpServer;
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSocket::setup()
{
    // Apply config
    applySetup();
}

void CommandSocket::applySetup()
{
    // Enable
    _isEnabled = configGetLong("enable", 0) != 0;

    // Port
    _port = configGetLong("socketPort", 80);

    // Protocol
    _protocol = configGetString("protocol", "RICSerial");

    // Check if already setup
#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
    if (_pTcpServer)
    {
        end();
        delete _pTcpServer;
        _pTcpServer = NULL;
    }

    // Setup server
    if (_isEnabled)
    {
        _pTcpServer = new AsyncServer(_port);
        _pTcpServer->onClient([this](void *s, AsyncClient *c) {
            if (c == NULL)
                return;
            c->setRxTimeout(3);
            this->addClient(c);

            // Verbose
            LOG_V(MODULE_PREFIX, "received socket client");

            // // TODO remove after finished with only
            // c->close(true);
            // c->free();
            // delete c;

            // AsyncWebServerRequest *r = new AsyncWebServerRequest((AsyncWebServer *)s, c);
            // if (r == NULL)
            // {
            //     c->close(true);
            //     c->free();
            //     delete c;
            // }
        }, this);
    }
#endif

    // Debug
    LOG_I(MODULE_PREFIX, "setup isEnabled %s TCP port %d", 
            _isEnabled ? "YES" : "NO", _port);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSocket::service()
{
    // Check if WiFi is connected and begin if so
    if ((!_begun) && networkSystem.isTCPIPConnected())
    {
        begin();
    }

//     // Take mutex
//     xSemaphoreTake(_clientMutex, portMAX_DELAY);

//     // Handle clients
//     for (AsyncClient* pClient : _clientList)
//     {
//         if (pClient->
//     }

//    // Return the mutex
//     xSemaphoreGive(_clientMutex);
// }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSocket::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
}

void CommandSocket::addProtocolEndpoints(ProtocolEndpointManager &endpointManager)
{
    // Register as a channel of protocol messages
    // TODO - really should have a separate protocolEndpointID for each socket that is connected???? - maybe need a pool???
    // TODO - otherwise it won't be possible to direct a response back to the right socket ???
    _protocolEndpointID = endpointManager.registerChannel(_protocol.c_str(),
            std::bind(&CommandSocket::sendMsg, this, std::placeholders::_1),
            modName(),
            std::bind(&CommandSocket::readyToSend, this));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSocket::begin()
{
#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
    if (_pTcpServer && !_begun)
    {
        _pTcpServer->setNoDelay(true);
        _pTcpServer->begin();
        _begun = true;

        // Debug
        LOG_I(MODULE_PREFIX, "has started on port %d", _port);
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// End
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSocket::end()
{
#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
    if (!_pTcpServer)
        return;
    _pTcpServer->end();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Client Handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
void CommandSocket::addClient(AsyncClient* pClient)
{
    // Add to client list if enough space
    if (_clientList.size() < MAX_CLIENTS)
    {
        pClient->onError([](void *r, AsyncClient* pClient, int8_t error)
        {
            // TODO - handle
            LOG_I(MODULE_PREFIX, "onError");
            // handleError(error);
        }, this);
        pClient->onAck([](void *r, AsyncClient* pClient, size_t len, uint32_t time)
        {
            // TODO - handle
            LOG_I(MODULE_PREFIX, "onAck");
            // handleAck(len, time); 
        }, this);
        pClient->onDisconnect([this](void *r, AsyncClient* pClient)
        {
            // TODO - handle
            LOG_I(MODULE_PREFIX, "onDisconnect");
            // handleDisconnect();
            delete pClient;
            this->removeFromClientList(pClient);
        }, this);
        pClient->onTimeout([](void *r, AsyncClient* pClient, uint32_t time)
        {
            // TODO - handle
            LOG_I(MODULE_PREFIX, "onTimeout");
            // handleTimeout(time);
        }, this);
        pClient->onData([this](void *r, AsyncClient* pClient, void *buf, size_t len)
        {
            // Received data
            uint8_t* pRxData = (uint8_t*)buf;

            // Debug
            // TODO - _DEBUG_COMMAND_SOCKET_ON_DATA
            // char outBuf[400];
            // strcpy(outBuf, "");
            // char tmpBuf[10];
            // for (int i = 0; i < len; i++)
            // {
            //     sprintf(tmpBuf, "%02x ", pRxData[i]);
            //     strlcat(outBuf, tmpBuf, sizeof(outBuf));
            // }
            // LOG_I(MODULE_PREFIX, "onData RX len %d %s", len, outBuf);

            // Send the message to the ProtocolEndpointManager
            if (getProtocolEndpointManager())
                getProtocolEndpointManager()->handleInboundMessage(this->_protocolEndpointID, pRxData, len);

            // Handle the data
            // handleData(buf, len);
        }, this);
        pClient->onPoll([](void *r, AsyncClient* pClient)
        {
            // TODO - handle
            LOG_I(MODULE_PREFIX, "onPoll");
            // handlePoll(); 
        }, this);

        // Take mutex
        xSemaphoreTake(_clientMutex, portMAX_DELAY);
        // Add to list of clients
        _clientList.push_back(pClient);
        // Return the mutex
        xSemaphoreGive(_clientMutex);
    }
    else
    {
        // TODO remove after finished with only
        pClient->close(true);
        pClient->free();
        delete pClient;
    }
}

void CommandSocket::removeFromClientList(AsyncClient* pClient)
{
    // Take mutex
    xSemaphoreTake(_clientMutex, portMAX_DELAY);
    // Remove from list of clients
    _clientList.remove(pClient);
    // Return the mutex
    xSemaphoreGive(_clientMutex);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message over socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandSocket::sendMsg(ProtocolEndpointMsg& msg)
{
    // LOG_D(MODULE_PREFIX, "sendBLEMsg channelID %d, direction %s msgNum %d, len %d",
    //         msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen());
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ready to send indicator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandSocket::readyToSend()
{
    // TODO - handle ready to send
    return true;
}
