// Bus Raider
// Rob Dobson 2019

#pragma once

#include "stdint.h"
#include "CommandInterface/CommandHandler.h"

class Display;
class UartMaxi;

class BusRaiderApp
{
public:

    BusRaiderApp(Display& display, UartMaxi& uart);

    void init();
    void initUSB();
    void service();

private:
    bool _lastActivityTickerState;

    // Status update rate and last timer
    static const uint32_t PI_TO_ESP32_STATUS_UPDATE_RATE_MS = 1000;
    uint32_t _statusUpdateStartUs;

    // Immediate mode
    static const int IMM_MODE_LINE_MAXLEN = 100;
    bool _immediateMode = false;
    char _immediateModeLine[IMM_MODE_LINE_MAXLEN+1];
    int _immediateModeLineLen = 0;

    // ESP32 status update
    uint32_t _esp32StatusUpdateStartUs;

    // Comms socket
    static CommsSocketInfo _commsSocket;

    // CommandHandler
    CommandHandler _commandHandler;

    // Display
    Display& _display;

    // UART
    UartMaxi& _uart;

    // Singleton
    static BusRaiderApp* _pApp;

    // Cached Pi status - as sent on change
    static const int MAX_PI_STATUS_MSG_LEN = 2000;
    char _piStatusCached[MAX_PI_STATUS_MSG_LEN];
    void getPiStatus(char* pRespJson, int maxRespLen);

    // Cached ESP32 status - received from ESP32
    static const int MAX_ESP_HEALTH_STR = 1000;
    static const int MAX_IP_ADDR_STR = 30;
    static const int MAX_WIFI_CONN_STR = 30;
    static const int MAX_WIFI_SSID_STR = 100;
    static const int MAX_ESP_VERSION_STR = 100;
    char _esp32IPAddress[MAX_IP_ADDR_STR];
    char _esp32WifiConnStr[MAX_WIFI_CONN_STR];
    char _esp32WifiSSID[MAX_WIFI_SSID_STR];
    char _esp32ESP32Version[MAX_ESP_VERSION_STR];
    bool _esp32IPAddressValid;
    static const uint32_t ESP32_TO_PI_STATUS_UPDATE_MAX_MS = 5000;
    uint32_t _esp32StatusLastRxUs;

    // Cached last machine command from ESP32
    char _esp32LastMachineCmd[CommandHandler::MAX_MC_SET_JSON_LEN];
    uint32_t _esp32LastMachineValid;
    uint32_t _esp32LastMachineReqUs;
    static const uint32_t ESP32_LAST_MC_REQ_MS = 5000;

    // Updates, etc
    static const int STATUS_UPDATE_TIME_MS = 1000;
    void clear();
    void statusDisplayUpdate();
    static void putToSerial(const uint8_t* pBuf, int len);
    static void serviceGetFromSerial();
    static void usbKeypressHandlerStatic(unsigned char ucModifiers, const unsigned char rawKeys[6]);
    void usbKeypressHandler(unsigned char ucModifiers, const unsigned char rawKeys[6]);
    static bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                char* pRespJson, int maxRespLen);
    void storeESP32StatusInfo(const char* pCmdJson);
};
