/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandSocket
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>
#include <list>

// #define TEST_USING_LOW_LEVEL_LWIP

// #define USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
#include <AsyncTCP.h>
#endif

class ProtocolEndpointMsg;

class CommandSocket : public SysModBase
{
public:
    // Constructor/destructor
    CommandSocket(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~CommandSocket();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager &endpointManager) override final;

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager &endpointManager) override final;

private:
    // Helpers
    void applySetup();
    void begin();
    void end();
    bool sendMsg(ProtocolEndpointMsg& msg);
    bool readyToSend(uint32_t channelID);

    // Vars
    bool _isEnabled;
    uint32_t _port;
    bool _begun;
    String _protocol;

    // EndpointID used to identify this message channel to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;

    // Protocol message handling
    static const uint32_t INBOUND_BLOCK_MAX_DEFAULT = 5000;
    static const uint32_t INBOUND_QUEUE_MAX_DEFAULT = 20;
    static const uint32_t OUTBOUND_BLOCK_MAX_DEFAULT = 5000;
    static const uint32_t OUTBOUND_QUEUE_MAX_DEFAULT = 5;        

#ifdef USE_ASYNC_SOCKET_FOR_COMMAND_SOCKET
    // Socket server
    AsyncServer *_pTcpServer;

    // Client connected
    void addClient(AsyncClient* pClient);
    
    // Remove client
    void removeFromClientList(AsyncClient* pClient);

    // Client list
    static const uint32_t MAX_CLIENTS = 2;
    std::list<AsyncClient*> _clientList;

    // Mutex controlling access to clients
    SemaphoreHandle_t _clientMutex;
#endif
#ifdef TEST_USING_LOW_LEVEL_LWIP
    static void tcpServerTask(void *pvParameters);
#endif

    // // Handles websocket events
    // static void webSocketCallback(uint8_t num, WEBSOCKET_TYPE_t type, const char* msg, uint64_t len);
};
