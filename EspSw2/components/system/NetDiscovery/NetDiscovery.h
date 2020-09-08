/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Net Discovery
// Enables discovery
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include "NetworkSystem.h"

#define USE_FREERTOS_TASK 1

class ConfigBase;
class RestAPIEndpointManager;

class NetDiscovery : public SysModBase
{
public:
    NetDiscovery(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Get status JSON
    virtual String getStatusJSON() override final;

private:
    // Helpers
    void applySetup();
    
    // Enabled
    bool _isDiscoveryEnabled;

    // UDP broadcast port
    static const uint32_t DEFAULT_UDP_BROADCAST_PORT = 4000;
    static uint32_t _udbBroadcastPort;

#ifdef USE_FREERTOS_TASK
    // Task
    TaskHandle_t _discoveryTaskHandle;
    static void discoveryUDPTaskWorker(void* pvParameters);
#endif

};
