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

    // Telnet server
    AsyncTelnetServer* _pTelnetServer;

    // TCP interfaces for DeZog and TCP-HDLC
    RemoteDebugProtocolServer* _DeZogTCPServer;
    uint32_t _DeZogCommandIndex;
    RemoteDebugProtocolServer* _pTCPHDLCServer;
    uint32_t _rdpCommandIndex;
    // HDLC processor for RDP
    MiniHDLC _miniHDLCForRDPTCP;

    // REST API
    RestAPIEndpoints* _pRestAPIEndpoints;

    // File manager
    FileManager* _pFileManager;

    // Max buffer sizes
    static const int MAX_COMMAND_LEN = 10000;

    // Status request
    uint32_t _cachedStatusRequestMs;
    static const int TIME_BETWEEN_STATUS_REQS_MS = 10000;

    // Command response
    static const int MAX_WAIT_FOR_CMD_RESPONSE_MS = 200;
    String _cmdResponseBuf;
    bool _cmdResponseNew;

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

    // Frame handlers for RDP
    void hdlcRxFrameTCP(const uint8_t *framebuffer, unsigned framelength);
    void hdlcTxCharTCP(uint8_t ch);

    // Hardware version
    static const int HW_VERSION_DETECT_V20_IN_PIN = 12;
    static const int HW_VERSION_DETECT_V22_IN_PIN = 5;
    static const int HW_VERSION_DETECT_OUT_PIN = 14;
    int _hwVersion;
    static const int ESP_HW_VERSION_DEFAULT = 20;
    void detectHardwareVersion();

public:
    MachineInterface();

    // Setup
    void setup(ConfigBase &config, 
                WebServer *pWebServer, 
                CommandSerial* pCommandSerial,
                AsyncTelnetServer* pTelnetServer, 
                RemoteDebugProtocolServer* pDeZogTCPServer, 
                RemoteDebugProtocolServer* pTCPHDLCServer, 
                RestAPIEndpoints* pRestAPIEndpoints, 
                FileManager* pFileManager);
                
    void service();

    // Handle a frame received from the Pi
    void handleFrameRxFromPi(const uint8_t *framebuffer, int framelength);

    const char *getStatus();
    void handleDemoModeButtonPress(int buttonLevel);

    void sendKeyToTarget(int keyCode);

    bool sendTargetCommand(const String& cmd, const String& reqStr, String& resp, bool waitForResponse=true);

    int getHwVersion()
    {
        return _hwVersion;
    }
};
