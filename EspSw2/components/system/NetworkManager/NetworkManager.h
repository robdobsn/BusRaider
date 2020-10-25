/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Network Manager
// Handles state of WiFi system, retries, etc and Ethernet
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include "NetworkSystem.h"

class ConfigBase;
class RestAPIEndpointManager;

class NetworkManager : public SysModBase
{
public:
    NetworkManager(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& pEndpoints) override final;

    // Get status JSON
    virtual String getStatusJSON() override final;

    // Get debug string
    virtual String getDebugStr() override final;

private:
    // Singleton NetworkManager
    static NetworkManager* _pNetworkManager;
    
    // Last connection status
    NetworkSystem::ConnStateCode _prevConnState;
    
    // Helpers
    void applySetup();
    void apiWifiSet(const String &reqStr, String &respStr);
    void apiWifiClear(const String &reqStr, String &respStr);
    void apiWiFiPause(const String &reqStr, String& respStr);
};
