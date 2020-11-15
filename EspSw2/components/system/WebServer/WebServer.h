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
    void webSocketSetup(const String& websocketURL);

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

    // Server config
    bool _accessControlAllowOriginAll;
    bool _webServerEnabled;
    uint32_t _port;
    String _restAPIPrefix;

    // Web server setup
    bool _isWebServerSetup;

    // Server
    RdWebServer _rdWebServer;

    // Singleton
    static WebServer* _pThisWebServer;
};
