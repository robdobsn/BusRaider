/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// NetworkSystem 
// Handles WiFi / Ethernet and IP
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "NetworkSystem.h"

static const char* MODULE_PREFIX = "NetworkSystem";

// Global object
NetworkSystem networkSystem;

// Retries
int NetworkSystem::_numConnectRetries = 0;
EventGroupHandle_t NetworkSystem::_wifiRTOSEventGroup;
String NetworkSystem::_staSSID;
String NetworkSystem::_wifiIPV4Addr;

// Paused
bool NetworkSystem::_isPaused = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NetworkSystem::NetworkSystem()
{
    // Vars
    _isEthEnabled = false;
    _isWiFiEnabled = false;
    _netifStarted = false;
    _wifiSTAStarted = false;

#ifdef PRIVATE_EVENT_LOOP
    // RTOS event loop
    _wifiEventLoopHandle = NULL;
#endif

    // Create the default event loop
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK)
        LOG_E(MODULE_PREFIX, "failed to create default event loop");

    // Create an RTOS event group to record events
    _wifiRTOSEventGroup = xEventGroupCreate();

    // Not initially connected
    xEventGroupClearBits(_wifiRTOSEventGroup, WIFI_CONNECTED_BIT);
    xEventGroupClearBits(_wifiRTOSEventGroup, IP_CONNECTED_BIT);
    xEventGroupClearBits(_wifiRTOSEventGroup, WIFI_FAIL_BIT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkSystem::setup(bool enableWiFi, bool enableEthernet, const char* defaultHostname)
{
    LOG_I(MODULE_PREFIX, "setup enableWiFi %s defaultHostname %s", enableWiFi ? "YES" : "NO", defaultHostname);

    _defaultHostname = defaultHostname;
    // Start netif if not done already
    if ((enableWiFi || enableEthernet) && (!_netifStarted))
    {
#ifdef USE_IDF_V4_1_NETIF_METHODS
        if (esp_netif_init() == ESP_OK)
            _netifStarted = true;
        else
            LOG_E(MODULE_PREFIX, "could not start netif");
#else
        tcpip_adapter_init();
        _netifStarted = true;
#endif
    }
    _isEthEnabled = enableEthernet;
    _isWiFiEnabled = enableWiFi;

    // Start WiFi STA if required
    if (_netifStarted && _isWiFiEnabled)
    {
        // Station mode
        startStationMode();

        // Connect
        if (!isWiFiStaConnectedWithIP())
        {
            LOG_D(MODULE_PREFIX, "applySetup caling connect");
            wifi_config_t configFromNVS;
            esp_err_t err = esp_wifi_get_config(ESP_IF_WIFI_STA, &configFromNVS);
            if (err == ESP_OK)
            {
                esp_wifi_set_config(ESP_IF_WIFI_STA, &configFromNVS);
                configFromNVS.sta.ssid[sizeof(configFromNVS.sta.ssid)-1] = 0;
                LOG_I(MODULE_PREFIX, "applySetup from NVS ... connect to ssid %s", configFromNVS.sta.ssid);
            }
            else
            {
                LOG_W(MODULE_PREFIX, "applySetup failed to get config from NVS");
            }
            esp_wifi_connect();
        }
        _isPaused = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool NetworkSystem::isWiFiStaConnectedWithIP()
{
    // Use the clear bits function with nothing to clear as a way to get the current value
    uint connBits = xEventGroupClearBits(_wifiRTOSEventGroup, 0);
    return (connBits & WIFI_CONNECTED_BIT) && (connBits & IP_CONNECTED_BIT);
}

bool NetworkSystem::isTCPIPConnected()
{
    // TODO V4.1 add ethernet support when available in netif
    // Use the clear bits function with nothing to clear as a way to get the current value
    uint connBits = xEventGroupClearBits(_wifiRTOSEventGroup, 0);
    return (connBits & IP_CONNECTED_BIT);
}

String NetworkSystem::getWiFiIPV4AddrStr()
{
    return _wifiIPV4Addr;
}

// Connection codes
enum ConnStateCode
{
    CONN_STATE_NONE,
    CONN_STATE_WIFI_BUT_NO_IP,
    CONN_STATE_WIFI_AND_IP
};

String NetworkSystem::getConnStateCodeStr(NetworkSystem::ConnStateCode connStateCode)
{
    switch(connStateCode)
    {
        case CONN_STATE_WIFI_BUT_NO_IP: return "WiFiNoIP";
        case CONN_STATE_WIFI_AND_IP: return "WiFiAndIP";
        case CONN_STATE_NONE: 
        default: 
            return "None";
    }
}

// Get conn state code
NetworkSystem::ConnStateCode NetworkSystem::getConnState()
{
    // Use the clear bits function with nothing to clear as a way to get the current value
    uint connBits = xEventGroupClearBits(_wifiRTOSEventGroup, 0);
    if ((connBits & WIFI_CONNECTED_BIT) && (connBits & IP_CONNECTED_BIT))
        return CONN_STATE_WIFI_AND_IP;
    else if (connBits & WIFI_CONNECTED_BIT)
        return CONN_STATE_WIFI_BUT_NO_IP;
    return CONN_STATE_NONE;    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkSystem::wifiEventHandler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
    LOG_I(MODULE_PREFIX, "============================= WIFI EVENT base %d id %d", (int)event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        LOG_I(MODULE_PREFIX, "STA_START ... calling connect");
        esp_wifi_connect();
        LOG_I(MODULE_PREFIX, "STA_START ... connect returned");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        uint32_t ssidLen = event->ssid_len;
        if (ssidLen > 32)
            ssidLen = 32;
        char ssidStr[33];
        strlcpy(ssidStr, (const char*)(event->ssid), ssidLen+1);
        _staSSID = ssidStr;
        xEventGroupSetBits(_wifiRTOSEventGroup, WIFI_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // Handle pause
        if (!_isPaused)
        {
            if ((WIFI_CONNECT_MAX_RETRY < 0) || (_numConnectRetries < WIFI_CONNECT_MAX_RETRY))
            {
                LOG_W(MODULE_PREFIX, "disconnected, retry to connect to the AP");
                if (esp_wifi_disconnect() == ESP_OK)
                {
                    esp_wifi_connect();
                }
                _numConnectRetries++;
            }
            else
            {
                xEventGroupSetBits(_wifiRTOSEventGroup, WIFI_FAIL_BIT);
            }
            LOG_W(MODULE_PREFIX, "disconnected, connect failed");
            _staSSID = "";
        }
        xEventGroupClearBits(_wifiRTOSEventGroup, WIFI_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        LOG_I(MODULE_PREFIX, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        _numConnectRetries = 0;
        char ipAddrStr[32];
        sprintf(ipAddrStr, IPSTR, IP2STR(&event->ip_info.ip));
        _wifiIPV4Addr = ipAddrStr;
        IP2STR(&event->ip_info.ip);
        xEventGroupSetBits(_wifiRTOSEventGroup, IP_CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP)
    {
        LOG_W(MODULE_PREFIX, "lost ip");
        if (!_isPaused)
            _wifiIPV4Addr = "";
        xEventGroupClearBits(_wifiRTOSEventGroup, IP_CONNECTED_BIT);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Station Mode
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool NetworkSystem::startStationMode()
{
    // Check if already initialised
    if (_wifiSTAStarted)
        return true;

#ifdef PRIVATE_EVENT_LOOP
    // Create event loop if not already there
    if (!_wifiEventLoopHandle)
    {
        esp_event_loop_args_t eventLoopArgs = {
            .queue_size = 10,
            .task_name = "WiFiEvLp",
            .task_priority = configMAX_PRIORITIES,
            .task_stack_size = 2048,
            .task_core_id = CONFIG_WIFI_EVENT_LOOP_CORE,
        };
        esp_err_t err = esp_event_loop_create(&eventLoopArgs, &_wifiEventLoopHandle);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "failed to start event loop");
            return false;
        }            
    }
#endif

    // Create the default WiFi STA
#ifdef USE_IDF_V4_1_NETIF_METHODS
    esp_netif_create_default_wifi_sta();
#endif

    // Setup a config to initialise the WiFi resources
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "startStationMode failed to init wifi err %d", err);
        return false;
    }

    // Attach event handlers
#ifdef PRIVATE_EVENT_LOOP
    esp_event_handler_register_with(_wifiEventLoopHandle, WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL);
    esp_event_handler_register_with(_wifiEventLoopHandle, IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, NULL);
    esp_event_handler_register_with(_wifiEventLoopHandle, IP_EVENT, IP_EVENT_STA_LOST_IP, &wifiEventHandler, NULL);
#else
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &wifiEventHandler, NULL);
#endif

    // Set storage
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);

    // Set mode
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "startStationMode failed to set mode err %d", err);
        return false;
    }

    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        LOG_E(MODULE_PREFIX, "startStationMode failed to start WiFi err %d", err);
        return false;
    }

    LOG_I(MODULE_PREFIX, "startStationMode init complete");

    // Init ok
    _wifiSTAStarted = true;
    return true;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configure WiFi
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool NetworkSystem::configureWiFi(const String& ssid, const String& pw, const String& hostname)
{
    if (hostname.length() == 0)
        _hostname = _defaultHostname;
    else
        _hostname = hostname;

    // Check if both SSID and pw have now been set
    if (ssid.length() != 0 && pw.length() != 0)
    {
        // Populate configuration for WiFi
        wifi_config_t wifiConfig = {
            .sta = {
                {.ssid = ""},
                {.password = ""},
                .scan_method = WIFI_FAST_SCAN,
                .bssid_set = 0,
                {.bssid = ""},
                .channel = 0,
                .listen_interval = 0,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                .threshold = {.rssi = 0, .authmode = WIFI_AUTH_OPEN}
#ifdef USE_V4_0_1_PLUS_NET_STRUCTS                
                ,
                .pmf_cfg = {.capable = 0, .required = 0}
#endif
                }};
        strlcpy((char *)wifiConfig.sta.ssid, ssid.c_str(), 32);
        strlcpy((char *)wifiConfig.sta.password, pw.c_str(), 64);

        // Set configuration
        esp_err_t err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifiConfig);
        if (err != ESP_OK)
        {
            LOG_E(MODULE_PREFIX, "startStationMode *** WiFi failed to set configuration err %s (%d) ***", esp_err_to_name(err), err);
            return false;
        }

        LOG_I(MODULE_PREFIX, "WiFi Credentials Set SSID %s hostname %s", ssid.c_str(), hostname.c_str());

        // Connect
        if (esp_wifi_disconnect() == ESP_OK)
        {
            esp_wifi_connect();
        }

        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear credentials
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

esp_err_t NetworkSystem::clearCredentials()
{
    // Restore to system defaults
    esp_err_t err = esp_wifi_restore();
    if (err == ESP_OK)
        LOG_I(MODULE_PREFIX, "apiWifiClear CLEARED WiFi Credentials");
    else
        LOG_W(MODULE_PREFIX, "apiWifiClear Failed to clear WiFi credentials esp_err %s (%d)", esp_err_to_name(err), err);
    return err;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pause WiFi operation - used to help with BLE vs WiFi contention in ESP32
// (they share the same radio)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetworkSystem::pauseWiFi(bool pause)
{
    // Check if pause or resume
    if (pause)
    {
        // Check already paused
        if (_isPaused)
            return;

        // Disconnect
        esp_wifi_disconnect();

        // Debug
        LOG_I(MODULE_PREFIX, "pauseWiFi - WiFi disconnected");
    }
    else
    {
        // Check already unpaused
        if (!_isPaused)
            return;

        // Connect
        esp_wifi_connect();
        _numConnectRetries = 0;

        // Debug
        LOG_I(MODULE_PREFIX, "pauseWiFi - WiFi reconnect requested");
    }
    _isPaused = pause;
}
