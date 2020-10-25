/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Network Manager
// Handles state of WiFi system, retries, etc and Ethernet
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "NetworkManager.h"
#include <Utils.h>
#include <ESPUtils.h>
#include <ConfigNVS.h>
#include <RestAPIEndpointManager.h>
#include <SysManager.h>

// Log prefix
static const char *MODULE_PREFIX = "NetworkManager";

// Singleton network manager
NetworkManager* NetworkManager::_pNetworkManager = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NetworkManager::NetworkManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _prevConnState = NetworkSystem::CONN_STATE_NONE;

    // Singleton
    _pNetworkManager = this;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::setup()
{
    applySetup();
}

void NetworkManager::applySetup()
{
    // Extract info from config
    bool isWiFiEnabled = (configGetLong("WiFiEnabled", 0) != 0);
    bool isEthEnabled = (configGetLong("EthEnabled", 0) != 0);
    String defaultHostname = configGetString("defaultHostname", "");
    networkSystem.setup(isWiFiEnabled, isEthEnabled, defaultHostname.c_str());

    // Debug
    LOG_D(MODULE_PREFIX, "setup isEnabled %s defaultHostname %s", isWiFiEnabled ? "YES" : "NO", defaultHostname.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::service()
{
    // Check for status change
    NetworkSystem::ConnStateCode connState = networkSystem.getConnState();
    if (_prevConnState != connState)
    {
        // Inform hooks of status change
        if (_pNetworkManager)
            _pNetworkManager->executeStatusChangeCBs();
        _prevConnState = connState;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String NetworkManager::getStatusJSON()
{

    NetworkSystem::ConnStateCode connState = networkSystem.getConnState();
    char statusStr[300];
    snprintf(statusStr, sizeof(statusStr), 
                "{\"rslt\":\"ok\",\"isConn\":%d,\"isPaused\":%d,\"connState\":\"%s\",\"SSID\":\"%s\",\"IP\":\"%s\",\"Hostname\":\"%s\",\"WiFiMAC\":\"%s\"}", 
                connState != NetworkSystem::CONN_STATE_NONE,
                networkSystem.isPaused() ? 1 : 0, 
                networkSystem.getConnStateCodeStr(connState).c_str(),
                networkSystem.getSSID().c_str(),
                networkSystem.getWiFiIPV4AddrStr().c_str(),
                networkSystem.getHostname().c_str(),
                getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":").c_str());
    return statusStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String NetworkManager::getDebugStr()
{
    if (networkSystem.isWiFiStaConnectedWithIP())
    {
        // TODO - show hostname
        return "SSID " + networkSystem.getSSID() + " IP " + networkSystem.getWiFiIPV4AddrStr();
    }
    else if (networkSystem.isPaused())
    {
        return "WiFi Paused";
    }
    return "No WiFi";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::addRestAPIEndpoints(RestAPIEndpointManager &endpointManager)
{
    endpointManager.addEndpoint("w", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&NetworkManager::apiWifiSet, this, std::placeholders::_1, std::placeholders::_2),
                          "Setup WiFi SSID/password/hostname");
    endpointManager.addEndpoint("wc", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&NetworkManager::apiWifiClear, this, std::placeholders::_1, std::placeholders::_2),
                          "Clear WiFi settings");
    // endpointManager.addEndpoint("wax", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
    //                       std::bind(&NetworkManager::apiWifiExtAntenna, this, std::placeholders::_1, std::placeholders::_2),
    //                       "Set external WiFi Antenna");
    // endpointManager.addEndpoint("wai", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
    //                       std::bind(&NetworkManager::apiWifiIntAntenna, this, std::placeholders::_1, std::placeholders::_2),
    //                       "Set internal WiFi Antenna");
    endpointManager.addEndpoint("wifipause", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
            std::bind(&NetworkManager::apiWiFiPause, this, std::placeholders::_1, std::placeholders::_2), 
            "WiFi pause, wifipause/pause, wifipause/resume");
}

void NetworkManager::apiWifiSet(const String &reqStr, String &respStr)
{
    // LOG_I(MODULE_PREFIX, "apiWifiSet incoming %s", reqStr.c_str());

    // Get SSID - note that ? is valid in SSIDs so don't split on ? character
    String ssid = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1, false);
    // Get pw - as above
    String pw = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 2, false);
    // LOG_I(MODULE_PREFIX, "WiFi PW length %d PW %s", pw.length(), pw.c_str());
    // Get hostname - as above
    String hostname = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 3, false);

    // Debug
    if (ssid.length() > 0)
    {
        LOG_I(MODULE_PREFIX, "apiWifiSet SSID %s (len %d) hostname %s (len %d)", 
                        ssid.c_str(), ssid.length(), hostname.c_str(), hostname.length());
    }
    else
    {
        LOG_I(MODULE_PREFIX, "apiWifiSet SSID is not set");
    }

    // Configure WiFi
    bool rslt = networkSystem.configureWiFi(ssid, pw, hostname);
    Utils::setJsonBoolResult(reqStr.c_str(), respStr, rslt);
}

void NetworkManager::apiWifiClear(const String &reqStr, String &respStr)
{
    // Clear stored credentials back to default
    esp_err_t err = networkSystem.clearCredentials();

    // Debug
    LOG_I(MODULE_PREFIX, "apiWifiClear ResultOK %s", err == ESP_OK ? "Y" : "N");

    // Response
    if (err == ESP_OK)
    {
        Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);

        // Request a system restart
        if (getSysManager())
            getSysManager()->systemRestart();
        return;
    }
    Utils::setJsonErrorResult(reqStr.c_str(), respStr, esp_err_to_name(err));
}

// void NetworkManager::apiWifiExtAntenna(String &reqStr, String &respStr)
// {
//     LOG_I(MODULE_PREFIX, "Set external antenna - not supported");
//     Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
// }

// void NetworkManager::apiWifiIntAntenna(String &reqStr, String &respStr)
// {
//     LOG_I(MODULE_PREFIX, "Set internal antenna - not supported");
//     Utils::setJsonBoolResult(reqStr.c_str(), respStr, true);
// }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control WiFi pause on BLE connection
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkManager::apiWiFiPause(const String &reqStr, String& respStr)
{
    // Get pause arg
    String arg = RestAPIEndpointManager::getNthArgStr(reqStr.c_str(), 1, false);

    // Check
    if (arg.equalsIgnoreCase("pause"))
    {
        networkSystem.pauseWiFi(true);
    }
    else if (arg.equalsIgnoreCase("resume"))
    {
        networkSystem.pauseWiFi(false);
    }
}
