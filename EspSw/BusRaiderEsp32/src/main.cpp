// Bus Raider ESP 32
// Rob Dobson 2018

// API used for web, MQTT and BLE (future)
//   Query status:   /q                   - returns network status & app status (lock/unlock/open/closed/bell)
//   Get version:    /v                   - returns version info
//   Set WiFi:       /w/ssss/pppp/hhhh    - ssss = ssid, pppp = password - assumes WPA2, hhhh = hostname
//                                        - does not clear previous WiFi so clear first if required
//   Clear WiFi:     /wc                  - clears all stored SSID, etc
//   Set MQTT:       /mq/ss/ii/oo/pp      - ss = server, ii and oo are in/out topics, pp = port
//                                        - in topics / should be replaced by ~ 
//                                        - (e.g. /devicename/in becomes ~devicename~in)
//   Check updates:  /checkupdate         - check for updates on the update server
//   Reset:          /reset               - reset device
//   Log level:      /loglevel/lll        - Logging level (for MQTT and HTTP)
//                                        - lll one of v (verbose), t (trace), n (notice), w (warning), e (error), f (fatal)
//   Log to MQTT:    /logmqtt/oo/topic    - Control logging to MQTT
//                                        - oo = 0 or 1 for off/on, topic is the topic logging messages are sent to
//   Log to HTTP:    /loghttp/oo/ip/po/ur - Control logging to HTTP
//                                        - oo = 0 or 1 for off/on
//                                        - ip is the IP address of the computer to log to (or hostname) and po is the port
//                                        - ur is the HTTP url logging messages are POSTed to

// System type
const char* systemType = "BusRaiderESP32";

// System version
const char* systemVersion = "1.000.003";

// Build date
const char* buildDate = __DATE__;

// Build date
const char* buildTime = __TIME__;

// Arduino
#include <Arduino.h>

// Logging
#include <ArduinoLog.h>

// WiFi
#include <WiFi.h>

// Utils
#include <Utils.h>

// Status LED
#include "StatusLed.h"
StatusLed wifiStatusLed;

// Config
#include "ConfigNVS.h"

// // WiFi Manager
#include "WiFiManager.h"
WiFiManager wifiManager;

// API Endpoints
#include "RestAPIEndpoints.h"
RestAPIEndpoints restAPIEndpoints;

// Web server
#include "WebServer.h"
WebServer webServer;

// MQTT
#include "MQTTManager.h"
MQTTManager mqttManager(wifiManager, restAPIEndpoints);

// Firmware update
#include <RdOTAUpdate.h>
RdOTAUpdate otaUpdate;

// Hardware config
static const char *hwConfigJSON = {
    "{"
    "\"unitName\": \"BusRaiderRSP32\","
    "\"wifiEnabled\":1,"
    "\"mqttEnabled\":0,"
    "\"webServerEnabled\":1,"
    "\"webServerPort\":80,"
    "\"OTAUpdate\":{\"enabled\":0,\"server\":\"someserver\",\"port\":5076},"
    "\"wifiLed\":{\"ledPin\":\"13\",\"ledOnMs\":200,\"ledShortOffMs\":200,\"ledLongOffMs\":750},"
    "\"serialConsole\":{\"portNum\":0},"
    "\"commandSerial\":{\"portNum\":1}"
    "}"
};

// Config for hardware
ConfigBase hwConfig(hwConfigJSON);

// Config for WiFi
ConfigNVS wifiConfig("wifi", 100);

// Config for MQTT
ConfigNVS mqttConfig("mqtt", 200);

// Config for network logging
ConfigNVS netLogConfig("netLog", 200);

// Comms (with Pi)
#include "CommandSerial.h"
CommandSerial commandSerial;

// NetLog
#include "NetLog.h"
NetLog netLog(Serial, mqttManager, commandSerial);

// REST API System
#include "RestAPISystem.h"
RestAPISystem restAPISystem(wifiManager, mqttManager, otaUpdate, netLog, systemType, systemVersion);

// REST API BusRaider
#include "RestAPIBusRaider.h"
RestAPIBusRaider restAPIBusRaider(commandSerial);

// Debug loop used to time main loop
#include "DebugLoopTimer.h"

// Debug loop timer and callback function
void debugLoopInfoCallback(String &infoStr)
{
    if (wifiManager.isEnabled())
        infoStr = wifiManager.getHostname() + " SSID " + WiFi.SSID() + " IP " + WiFi.localIP().toString() + " Heap " + String(ESP.getFreeHeap());
    else
        infoStr = "WiFi Disabled, Heap " + String(ESP.getFreeHeap());
}
DebugLoopTimer debugLoopTimer(10000, debugLoopInfoCallback);

// Serial console - for configuration
#include "SerialConsole.h"
SerialConsole serialConsole;

/*
void fileUploadHandler(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (request && (request->contentLength() > 0))
        commandSerial.fileUploadPart(filename, request->contentLength(), index, data, len, final);
}
*/

// Setup
void setup()
{
    // Logging
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_TRACE, &Serial);

    // Message
    Log.notice("%s %s (built %s %s)\n", systemType, systemVersion, buildDate, buildTime);

    // Status Led
    wifiStatusLed.setup(hwConfig, "wifiLed");

    // WiFi Config
    wifiConfig.setup();

    // MQTT Config
    mqttConfig.setup();

    // NetLog Config
    netLogConfig.setup();

    // Firmware update
    otaUpdate.setup(hwConfig, systemType, systemVersion);

    // Add API endpoints
    restAPISystem.setup(restAPIEndpoints);
    restAPIBusRaider.setup(restAPIEndpoints);

    // Serial console
    serialConsole.setup(hwConfig, restAPIEndpoints);

    // WiFi Manager
    wifiManager.setup(hwConfig, &wifiConfig, systemType, &wifiStatusLed);

    // Web server
    webServer.setup(hwConfig);
    webServer.addEndpoints(restAPIEndpoints);

    // MQTT
    mqttManager.setup(hwConfig, &mqttConfig);

    // Setup CommandSerial
    commandSerial.setup(hwConfig);

    // Network logging
    netLog.setup(&netLogConfig, wifiManager.getHostname().c_str());
/*
    // File upload
    String uploadResponseStr;
    Utils::setJsonBoolResult(uploadResponseStr, true);
    webServer.addFileUploadHandler(fileUploadHandler, uploadResponseStr.c_str());
*/

    // Add debug blocks
    debugLoopTimer.blockAdd(0, "Web");
    debugLoopTimer.blockAdd(1, "Console");
    debugLoopTimer.blockAdd(2, "MQTT");
    debugLoopTimer.blockAdd(3, "OTA");
    debugLoopTimer.blockAdd(4, "Command");
}

// Loop
void loop()
{

    // Debug loop Timing
    debugLoopTimer.service();

    // Service WiFi
    wifiManager.service();

    // Service the web server
    if (wifiManager.isConnected())
    {
        // Begin the web server
        debugLoopTimer.blockStart(0);
        webServer.begin(true);
        debugLoopTimer.blockEnd(0);
    }

    // Service the status LED
    wifiStatusLed.service();

    // Service the system API (restart)
    restAPISystem.service();

    // Serial console
    debugLoopTimer.blockStart(1);
    serialConsole.service();
    debugLoopTimer.blockEnd(1);

    // Service MQTT
    debugLoopTimer.blockStart(2);
    mqttManager.service();
    debugLoopTimer.blockEnd(2);

    // Service OTA Update
    debugLoopTimer.blockStart(3);
    otaUpdate.service();
    debugLoopTimer.blockEnd(3);

    // Service NetLog
    netLog.service();

    // Service CommandSerial
    debugLoopTimer.blockStart(4);
    commandSerial.service();
    debugLoopTimer.blockEnd(4);
}