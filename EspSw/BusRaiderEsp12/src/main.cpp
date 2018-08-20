// Bus Raider ESP 12
// Rob Dobson 2018

// System type
const char* systemType = "BusRaiderESP12";

// System version
const char* systemVersion = "1.000.002";

// Build date
const char* buildDate = __DATE__;

// Build date
const char* buildTime = __TIME__;

// Arduino
#include <Arduino.h>

// Utils
#include <Utils.h>

// System debug
#include <SystemDebug.h>
SystemDebug systemDebug;

// Status LED
#include "StatusLed.h"
StatusLed wifiStatusLed;

// // WiFi Manager
#include "WiFiManager.h"
WiFiManager wifiManager;

// API Endpoints
#include "RestAPIEndpoints.h"
RestAPIEndpoints restAPIEndpoints;

// Web server
#include "WebServer.h"
WebServer webServer;

// Hardware config
static const char *hwConfigJSON = {
    "{"
    "\"unitName\": \"BusRaiderRSP12\","
    "\"wifiEnabled\":1,"
    "\"webServerEnabled\":1,"
    "\"webServerPort\":80,"
    "\"wifiLed\":{\"ledPin\":\"5\",\"ledOnMs\":200,\"ledShortOffMs\":200,\"ledLongOffMs\":750},"
    "\"serialConsole\":{\"portNum\":0},"
    "\"piComms\":{\"portNum\":1}"
    "}"
};

// Config for hardware
ConfigBase hwConfig(hwConfigJSON);

// Comms with Pi
#include "PiComms.h"
PiComms piComms;

// Config for WiFi
ConfigEEPROM configNV(0, 1000);

// REST API System
#include "RestAPISystem.h"
RestAPISystem restAPISystem(systemDebug, wifiManager);

// REST API BusRaider
#include "RestAPIBusRaider.h"
RestAPIBusRaider restAPIBusRaider(piComms);

// Debug loop used to time main loop
#include "DebugLoopTimer.h"

// Debug loop timer and callback function
void debugLoopInfoCallback(String &infoStr)
{
    if (wifiManager.isEnabled())
        infoStr = wifiManager.getHostname() + " SSID " + WiFi.SSID() + " IP " + WiFi.localIP().toString() + " Heap " + String(ESP.getFreeHeap());
    else
        infoStr = "Heap " + String(ESP.getFreeHeap());
}
DebugLoopTimer debugLoopTimer(10000, debugLoopInfoCallback);

// Serial console - for configuration
#include "SerialConsole.h"
SerialConsole serialConsole;

void fileUploadHandler(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (request && (request->contentLength() > 0))
        piComms.fileUploadPart(filename, request->contentLength(), index, data, len, final);
}

// Setup
void setup()
{
    // Logging
    Serial.begin(115200);
    Serial1.begin(115200);
    Log.begin(LOG_LEVEL_TRACE, &Serial);

    // Message
    Log.notice("%s %s (built %s %s)\n", systemType, systemVersion, buildDate, buildTime);

    // Status Led
    wifiStatusLed.setup(hwConfig, "wifiLed");

    // Config
    configNV.setup();

    // Pi Comms
    piComms.setup(hwConfig);

    // Add API endpoints
    restAPISystem.setup(restAPIEndpoints);
    restAPIBusRaider.setup(restAPIEndpoints);

    // Serial console
    serialConsole.setup(hwConfig, restAPIEndpoints);

    // WiFi Manager
    wifiManager.setup(hwConfig, &configNV, systemType, &wifiStatusLed);

    // Web server
    webServer.setup(hwConfig);
    webServer.addEndpoints(restAPIEndpoints);

    // File upload
    String uploadResponseStr;
    Utils::setJsonBoolResult(uploadResponseStr, true);
    webServer.addFileUploadHandler(fileUploadHandler, uploadResponseStr.c_str());

    // Add debug blocks
    debugLoopTimer.blockAdd(0, "Web");
    debugLoopTimer.blockAdd(1, "Serial");
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

    // Service the Pi Comms
    piComms.service();

    // Service the status LED
    wifiStatusLed.service();

    // Service the system API (restart)
    restAPISystem.service();
    
    // Service config
    configNV.service();

    // Serial console
    debugLoopTimer.blockStart(1);
    serialConsole.service();
    debugLoopTimer.blockEnd(1);

}
