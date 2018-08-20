// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include <SystemDebug.h>
#include <WiFiManager.h>
#include "RestAPIEndpoints.h"

// #define ENABLE_OTA_UPDATE 1
// #define ENABLE_MQTT_SERVER 1

class RestAPISystem
{
  private:
    bool _deviceRestartPending;
    unsigned long _deviceRestartMs;
    static const int DEVICE_RESTART_DELAY_MS = 1000;
    bool _updateCheckPending;
    unsigned long _updateCheckMs;
    // Delay before starting an update check
    // For some reason TCP connect fails if there is not a sufficient delay
    // But only when initiated from MQTT (web works ok)
    // 3s doesn't work, 5s seems ok
    static const int DEVICE_UPDATE_DELAY_MS = 7000;
    SystemDebug& _systemDebug;
    WiFiManager& _wifiManager;
    
  public:
    RestAPISystem(SystemDebug& systemDebug, WiFiManager& wifiManager) :
                 _systemDebug(systemDebug), _wifiManager(wifiManager)
    {
        _deviceRestartPending = false;
        _deviceRestartMs = 0;
        _updateCheckPending = false;
        _updateCheckMs = 0;
    }

    void service()
    {
        // Check restart pending
        if (_deviceRestartPending)
        {
            if (Utils::isTimeout(millis(), _deviceRestartMs, DEVICE_RESTART_DELAY_MS))
            {
                _deviceRestartPending = false;
                ESP.restart();
            }
        }
#ifdef ENABLE_OTA_UPDATE
        // Check for update pending
        if (_updateCheckPending)
        {
            if (Utils::isTimeout(millis(), _updateCheckMs, DEVICE_UPDATE_DELAY_MS))
            {
                _updateCheckPending = false;
                otaUpdate.requestUpdateCheck();
            }
        }
#endif
    }

    void apiWifiSet(String &reqStr, String &respStr)
    {
        bool rslt = false;
        // Get SSID
        String ssid = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        Log.trace("RestAPISystem: WiFi SSID %s\n", ssid.c_str());
        // Get pw
        String pw = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
        Log.trace("RestAPISystem: WiFi PW %s\n", pw.c_str());
        // Get hostname
        String hostname = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 3);
        Log.trace("RestAPISystem: Hostname %s\n", hostname.c_str());
        // Check if both SSID and pw have now been set
        if (ssid.length() != 0 && pw.length() != 0)
        {
            Log.notice("RestAPISystem: WiFi Credentials Added SSID %s\n", ssid.c_str());
            wifiManager.setCredentials(ssid, pw, hostname);
            rslt = true;
        }
        Utils::setJsonBoolResult(respStr, rslt);
    }

    void apiWifiClear(String &reqStr, String &respStr)
    {
        // Clear stored SSIDs
        // wifiManager.clearCredentials();
        Log.notice("RestAPISystem: Cleared WiFi Credentials\n");
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiWifiExtAntenna(String &reqStr, String &respStr)
    {
        Log.notice("RestAPISystem: Set external antenna - not supported\n");
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiWifiIntAntenna(String &reqStr, String &respStr)
    {
        Log.notice("RestAPISystem: Set internal antenna - not supported\n");
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiMQTTSet(String &reqStr, String &respStr)
    {
#ifdef ENABLE_MQTT_SERVER
        // Get Server
        String server = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        Log.trace("RestAPISystem: MQTTServer %s\n", server.c_str());
        // Get mqtt in topic
        String inTopic = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
        inTopic.replace("~", "/");
        Log.trace("RestAPISystem: MQTTInTopic %s\n", inTopic.c_str());
        // Get mqtt out topic
        String outTopic = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 3);
        outTopic.replace("~", "/");
        Log.trace("RestAPISystem: MQTTOutTopic %s\n", outTopic.c_str());
        // Get port
        int portNum = MQTTManager::DEFAULT_MQTT_PORT;
        String port = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 4);
        if (port.length() == 0)
            portNum = port.toInt();
        Log.trace("RestAPISystem: MQTTPort %n\n", portNum);
        mqttManager.setMQTTServer(server, inTopic, outTopic, portNum);
#endif
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiReset(String &reqStr, String& respStr)
    {
        // Register that a restart is required but don't restart immediately
        // as the acknowledgement would not get through
        Utils::setJsonBoolResult(respStr, true);

        // Restart the device
        _deviceRestartPending = true;
        _deviceRestartMs = millis();
    }

    void apiDebugMode(String &reqStr, String &respStr)
    {
        // Get debug Mode
        String debugMode = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
        Log.trace("RestAPISystem: Debug Mode %s\n", debugMode.c_str());
        _systemDebug.setMode(debugMode.toInt());
        Utils::setJsonBoolResult(respStr, true);
    }

    void apiCheckUpdate(String &reqStr, String& respStr)
    {
        // Register that an update check is required but don't start immediately
        // as the TCP stack doesn't seem to connect if the server is the same
        Utils::setJsonBoolResult(respStr, true);

        // Check for updates on update server
        _updateCheckPending = true;
        _updateCheckMs = millis();
    }

    void apiGetVersion(String &reqStr, String& respStr)
    {
        respStr = "{\"sysType\":\""+ String(systemType) + "\", \"version\":\"" + String(systemVersion) + "\"}";
    }

    void setup(RestAPIEndpoints &endpoints)
    {
        endpoints.addEndpoint("w", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiWifiSet, this, std::placeholders::_1, std::placeholders::_2),
                        "Setup WiFi SSID/password/hostname");
        endpoints.addEndpoint("wc", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiWifiClear, this, std::placeholders::_1, std::placeholders::_2),
                        "Clear WiFi settings");
        endpoints.addEndpoint("wax", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiWifiExtAntenna, this, std::placeholders::_1, std::placeholders::_2), 
                        "Set external WiFi Antenna");
        endpoints.addEndpoint("wai", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiWifiIntAntenna, this, std::placeholders::_1, std::placeholders::_2), 
                        "Set internal WiFi Antenna");
        endpoints.addEndpoint("mq", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiMQTTSet, this, std::placeholders::_1, std::placeholders::_2),
                        "Setup MQTT server/port/intopic/outtopic .. not ~ replaces / in topics");
        endpoints.addEndpoint("reset", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiReset, this, std::placeholders::_1, std::placeholders::_2), 
                        "Restart program");
        endpoints.addEndpoint("checkupdate", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiCheckUpdate, this, std::placeholders::_1, std::placeholders::_2), 
                        "Check for updates");
        endpoints.addEndpoint("v", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiGetVersion, this, std::placeholders::_1, std::placeholders::_2), 
                        "Get version info");
        endpoints.addEndpoint("debug", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET, 
                        std::bind(&RestAPISystem::apiDebugMode, this, std::placeholders::_1, std::placeholders::_2), 
                        "Enter debug mode");
        }

    String getWifiStatusStr()
    {
        if (WiFi.status() == WL_CONNECTED)
            return "C";
        if (WiFi.status() == WL_NO_SHIELD)
            return "4";
        if (WiFi.status() == WL_IDLE_STATUS)
            return "I";
        if (WiFi.status() == WL_NO_SSID_AVAIL)
            return "N";
        if (WiFi.status() == WL_SCAN_COMPLETED)
            return "S";
        if (WiFi.status() == WL_CONNECT_FAILED)
            return "F";
        if (WiFi.status() == WL_CONNECTION_LOST)
            return "L";
        return "D";
    }

    int reportHealth(int bitPosStart, unsigned long *pOutHash, String *pOutStr)
    {
        // Generate hash if required
        if (pOutHash)
        {
            if (_wifiManager.isEnabled())
            {
                unsigned long hashVal = (WiFi.status() == WL_CONNECTED);
                hashVal = hashVal << bitPosStart;
                *pOutHash += hashVal;
                *pOutHash ^= WiFi.localIP();
            }
        }
        // Generate JSON string if needed
        if (pOutStr)
        {
            if (_wifiManager.isEnabled())
            {
                byte mac[6];
                WiFi.macAddress(mac);
                String macStr = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" +
                                String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX);
                String sOut = "\"wifiIP\":\"" + WiFi.localIP().toString() + "\",\"wifiConn\":\"" + getWifiStatusStr() + "\","
                                                                                                                        "\"ssid\":\"" +
                            WiFi.SSID() + "\",\"MAC\":\"" + macStr + "\",\"RSSI\":" + String(WiFi.RSSI());
                *pOutStr = sOut;
            }
        }
        // Return number of bits in hash
        return 8;
    }
};
