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

#ifdef TEST_USING_LOW_LEVEL_LWIP
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <memory.h>
#endif

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
    _port = configGetLong("socketPort", 24);

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
            modName(),
            modName(),
            std::bind(&CommandSocket::sendMsg, this, std::placeholders::_1),
            [this](uint32_t channelID, bool& noConn) {
                return true;
            },
            INBOUND_BLOCK_MAX_DEFAULT,
            INBOUND_QUEUE_MAX_DEFAULT,
            OUTBOUND_BLOCK_MAX_DEFAULT,
            OUTBOUND_QUEUE_MAX_DEFAULT);
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
#ifdef TEST_USING_LOW_LEVEL_LWIP
    xTaskCreate(tcpServerTask, "CommandSockTCP", 10000, NULL, 5, NULL);
    // Debug
    LOG_I(MODULE_PREFIX, "has started on port %d", _port);
    _begun = true;
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
        if (xSemaphoreTake(_clientMutex, portMAX_DELAY) == pdTRUE)
        {
            // Add to list of clients
            _clientList.push_back(pClient);
            // Return the mutex
            xSemaphoreGive(_clientMutex);
        }
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
    if (xSemaphoreTake(_clientMutex, portMAX_DELAY) == pdTRUE)
    {
        // Remove from list of clients
        _clientList.remove(pClient);
        // Return the mutex
        xSemaphoreGive(_clientMutex);
    }
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message over socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandSocket::sendMsg(ProtocolEndpointMsg& msg)
{
    // LOG_D(MODULE_PREFIX, "sendBLEMsg channelID %d, msgType %s msgNum %d, len %d",
    //         msg.getChannelID(), msg.getMsgTypeAsString(msg.getMsgTypeCode()), msg.getMsgNumber(), msg.getBufLen());
    return true;
}

#ifdef TEST_USING_LOW_LEVEL_LWIP

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TCP Server Task
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandSocket::tcpServerTask(void *pvParameters)
{
    char rx_buffer[2000];	// char array to store received data
    char addr_str[128];		// char array to store client IP
    int bytes_received;		// immediate bytes received
    int addr_family;		// Ipv4 address protocol variable
    int _expFrameCount = 0;
    int _okRunCount = 0;
    char _remainBuf[4000];
    int _remainBufLen = 0;

    struct sockaddr_in destAddr;
    destAddr.sin_addr.s_addr = htonl(INADDR_ANY); //Change hostname to network byte order
    destAddr.sin_family = AF_INET;		//Define address family as Ipv4
    destAddr.sin_port = htons(24); 	    //Define PORT
    addr_family = AF_INET;				//Define address family as Ipv4
    inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

    /* Create TCP socket*/
    int socket_id = socket(addr_family, SOCK_STREAM, IPPROTO_TCP);
    if (socket_id < 0)
    {
        ESP_LOGE(MODULE_PREFIX, "Unable to create socket: errno %d", errno);
        return;
    }
    LOG_I(MODULE_PREFIX, "Socket created");

    /* Bind a socket to a specific IP + port */
    int bind_err = bind(socket_id, (struct sockaddr *)&destAddr, sizeof(destAddr));
    if (bind_err != 0)
    {
        ESP_LOGE(MODULE_PREFIX, "Socket unable to bind: errno %d", errno);
        return;
    }
    LOG_I(MODULE_PREFIX, "Socket bound");

    /* Begin listening for clients on socket */
    int listen_error = listen(socket_id, 3);
    if (listen_error != 0)
    {
        ESP_LOGE(MODULE_PREFIX, "Error occured during listen: errno %d", errno);
        return;
    }
    LOG_I(MODULE_PREFIX, "Socket listening");

    while (1)
    {
        struct sockaddr_in sourceAddr; // Large enough for IPv4
        uint addrLen = sizeof(sourceAddr);
        /* Accept connection to incoming client */
        int client_socket = accept(socket_id, (struct sockaddr *)&sourceAddr, &addrLen);
        if (client_socket < 0)
        {
            ESP_LOGE(MODULE_PREFIX, "Unable to accept connection: errno %d", errno);
            break;
        }
        LOG_I(MODULE_PREFIX, "Socket accepted");

        //Optionally set O_NONBLOCK
        //If O_NONBLOCK is set then recv() will return, otherwise it will stall until data is received or the connection is lost.
        //fcntl(client_socket,F_SETFL,O_NONBLOCK);

        // Clear rx_buffer, and fill with zero's
        bzero(rx_buffer, sizeof(rx_buffer));
        vTaskDelay(500 / portTICK_PERIOD_MS);
        while(1)
        {
            // LOG_I(MODULE_PREFIX, "Waiting for data");
            bytes_received = recv(client_socket, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // LOG_I(MODULE_PREFIX, "Received Data");

            // Error occured during receiving
            if (bytes_received < 0)
            {
                LOG_I(MODULE_PREFIX, "Error on rx");
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            // Connection closed
            else if (bytes_received == 0)
            {
                LOG_I(MODULE_PREFIX, "Connection closed");
                break;
            }
            // Data received
            else
            {
                // Add to remain buf
                if (bytes_received + _remainBufLen + 1 >= sizeof(_remainBuf))
                {
                    LOG_I(MODULE_PREFIX, "OVERFLOWED REMAIN BUF");
                    _remainBufLen = 0;
                    continue;
                }
                memcpy(_remainBuf+_remainBufLen, rx_buffer, bytes_received);
                _remainBufLen += bytes_received;
                _remainBuf[_remainBufLen] = 0;

                // Take from buffer
                while (_remainBufLen > 6)
                {
                    // Find end
                    char* endOfFrame = strstr(_remainBuf, "~");
                    if (endOfFrame == NULL)
                        break;

                    // Get frame count
                    uint32_t frameCount = 0;
                    uint32_t mul = 1;
                    for (int i = 0; i < 6; i++)
                    {
                        frameCount += (_remainBuf[5-i]-'0')*mul;
                        mul *= 10;
                    }

                    // rx_buffer[bytes_received] = 0; // Null-terminate whatever we received and treat like a string
                    LOG_I(MODULE_PREFIX, "Fr %d", frameCount);

                    // Check sequence
                    if (frameCount == _expFrameCount)
                    {
                        _expFrameCount++;
                        _okRunCount++;
                        if ((_expFrameCount % 10) == 9)
                        {
                            LOG_I(MODULE_PREFIX, "Rx frame %d ok run %d", frameCount, _okRunCount);
                        }
                    }
                    else
                    {
                        LOG_I(MODULE_PREFIX, "Expected %d got %d okRun %d", _expFrameCount, frameCount, _okRunCount);
                        _expFrameCount = frameCount+1;
                        _okRunCount = 0;
                    }

                    // Remove the frame
                    int frameLen = endOfFrame - _remainBuf + 1;
                    _remainBufLen -= frameLen;
                    memmove(_remainBuf, endOfFrame+1, _remainBufLen);
                    _remainBuf[_remainBufLen] = 0;
                }

                // Clear rx_buffer, and fill with zero's
                bzero(rx_buffer, sizeof(rx_buffer));

            }
        }
    }
    vTaskDelete(NULL);
}

#endif
