/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// NetworkSystem 
// Handles WiFi / Ethernet and IP
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "WString.h"

// #define PRIVATE_EVENT_LOOP 1
// #define USE_IDF_V4_1_NETIF_METHODS 1
#define USE_V4_0_1_PLUS_NET_STRUCTS 1

class NetworkSystem
{
public:
    NetworkSystem();

    // Setup 
    void setup(bool enableWiFi, bool enableEthernet, const char* hostname);

    bool isWiFiEnabled()
    {
        return _isWiFiEnabled;
    }
    // Station mode connected
    bool isWiFiStaConnectedWithIP();

    // Is TCPIP connected
    bool isTCPIPConnected();

    // Connection info
    uint32_t getWiFiIPV4Addr();
    String getWiFiIPV4AddrStr();

    String getHostname()
    {
        return _hostname;
    }

    String getSSID()
    {
        return _staSSID;
    }

    bool configureWiFi(const String& ssid, const String& pw, const String& hostname);

    // Connection codes
    enum ConnStateCode
    {
        CONN_STATE_NONE,
        CONN_STATE_WIFI_BUT_NO_IP,
        CONN_STATE_WIFI_AND_IP
    };
    static String getConnStateCodeStr(ConnStateCode connStateCode);

    // Get conn state code
    ConnStateCode getConnState();

    // Clear credentials
    esp_err_t clearCredentials();

    // Pause WiFi operation - used to help with BLE vs WiFi contention in ESP32
    // (they share the same radio)
    void pauseWiFi(bool pause);
    bool isPaused()
    {
        return _isPaused;
    }

private:

    // Enabled
    bool _isWiFiEnabled;
    bool _isEthEnabled;
    static bool _isPaused;

    // Started flags
    bool _netifStarted;
    bool _wifiSTAStarted;

    // WiFi connection details
    static String _staSSID;
    static String _wifiIPV4Addr;
    String _hostname;
    String _defaultHostname;

    // Connect retries
    static int _numConnectRetries; 

    // Retry max, -1 means try forever
    static const int WIFI_CONNECT_MAX_RETRY = -1;

#ifdef PRIVATE_EVENT_LOOP
    // Event loop for WiFi connection
    esp_event_loop_handle_t _wifiEventLoopHandle;
#endif

    // FreeRTOS event group to signal when we are connected
    // Code here is based on https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/station/main/station_example_main.c
    static EventGroupHandle_t _wifiRTOSEventGroup;

    // The event group allows multiple bits for each event
    static const uint32_t WIFI_CONNECTED_BIT = BIT0;
    static const uint32_t IP_CONNECTED_BIT = BIT1;
    static const uint32_t WIFI_FAIL_BIT = BIT2;

    bool startStationMode();
    static void wifiEventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
};

extern NetworkSystem networkSystem;
