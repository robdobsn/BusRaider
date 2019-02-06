// Machine Interface
// Rob Dobson 2018

// Provides some wiring for the Bus Raider
// The ESP32 has three serial interfaces:
// - programming/logging
// - Pi
// - Target serial (e.g. Z80 68B50)

// The CommandSerial.h file contains code which communicates with the Pi using and HDLC frame protocol
// when a frame is ready from the Pi it is handled by this MachineInterface
// This MachineInterface also handles the target serial port communication including telnet support

#pragma once

#include "WebServer.h"
#include "CommandSerial.h"
#include "AsyncTelnetServer.h"
#include "RemoteDebugProtocol.h"
#include "RestAPIEndpoints.h"
#include "DebounceButton.h"

// #define USE_WEBSOCKET_TERMINAL 1

class MachineInterface
{
private:
    // Cache for machine status so that it can be quickly returned
    // when needed
    String _cachedStatusJSON;

    // Serial
    HardwareSerial* _pTargetSerial;

    // Serial port details
    int _targetSerialPortNum;
    int _targetSerialBaudRate;

    // Web server ptr (copy)
    WebServer* _pWebServer;

    // Command serial ptr (copy)
    CommandSerial* _pCommandSerial;

    // Web socket for communication of keystrokes
#ifdef USE_WEBSOCKET_TERMINAL
    AsyncWebSocket* _pWebSocket;
#endif

    // Telnet server
    AsyncTelnetServer* _pTelnetServer;

    // Remote debug server
    RemoteDebugProtocolServer* _pRemoteDebugServer;
    int _rdpCommandIndex;

    // REST API
    RestAPIEndpoints* _pRestAPIEndpoints;

    // File manager
    FileManager* _pFileManager;

    // Max buffer sizes
    static const int MAX_COMMAND_LEN = 1500;

    // Demo handling
    DebounceButton _debounceButton;
    int _demoState;
    static const int DEMO_STATE_IDLE = 0;
    static const int DEMO_STATE_PRELOAD = 1;
    static const int DEMO_STATE_LOAD = 2;
    int _demoPreloadFileIdx;
    static const int DEMO_PRELOAD_MAX_FILES = 10;
    String _demoPreloadFilesJson;
    String _demoFileToRun;
    int _demoProgramIdx;

public:
    MachineInterface();

    // Setup
    void setup(ConfigBase &config, WebServer *pWebServer, CommandSerial* pCommandSerial,
                AsyncTelnetServer* pTelnetServer, RemoteDebugProtocolServer* pRemoteDebugProtocolServer,
                RestAPIEndpoints* pRestAPIEndpoints, FileManager* pFileManager);
                
    void service();

    // Handle a frame received from the Pi
    void handleFrameRxFromPi(const uint8_t *framebuffer, int framelength);

    const char *getStatus();
    void handleDemoModeButtonPress(int buttonLevel);

    // int convertEscKey(String& keyStr);

#ifdef USE_WEBSOCKET_TERMINAL
    void wsEventHandler(AsyncWebSocket *server,
                        AsyncWebSocketClient *client,
                        AwsEventType type,
                        void *arg, uint8_t *data,
                        size_t len);
#endif
};
