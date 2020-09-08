/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WebServer
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>

class WebServerResource;
class ProtocolEndpointMsg;

#include "RdWebServer.h"

class WebServer : public SysModBase
{
public:
    // Constructor/destructor
    WebServer(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);
    virtual ~WebServer();

    // Begin
    void beginServer();

    // Add resources to the web server
    void addStaticResources(const WebServerResource *pResources, int numResources);
    void serveStaticFiles(const char* baseUrl, const char* baseFolder, const char* cacheControl = NULL);
    
    // Async event handler (one-way text to browser)
    void enableAsyncEvents(const String& eventsURL);
    void sendAsyncEvent(const char* eventContent, const char* eventGroup);

    // Web sockets
    void webSocketOpen(const String& websocketURL);
    void webSocketSend(const uint8_t* pBuf, uint32_t len);

protected:
    // Setup
    virtual void setup();

    // Service - called frequently
    virtual void service();

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager);

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager& endpointManager);
    
private:
    // Helpers
    void addStaticResource(const WebServerResource *pResource, const char *pAliasPath = NULL);
    void configChanged();
    void applySetup();
    void setupEndpoints();
    bool sendWebSocketMsg(ProtocolEndpointMsg& msg);
    bool readyToSendWebSocketMsg();

    // Vars
    bool _accessControlAllowOriginAll;
    bool _webServerEnabled;
    uint32_t _port;
    String _restAPIPrefix;

    // EndpointID used to identify this message source to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;

    // Web server setup
    bool _isWebServerSetup;

    // Server
    RdWebServer _rdWebServer;

    // Singleton
    static WebServer* _pThisWebServer;

    // Handles websocket events
    static void webSocketCallback(RdWebSocketEventCode eventCode, const uint8_t* pBuf, uint32_t bufLen);
};
