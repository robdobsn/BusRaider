// Bus Raider
// Rob Dobson 2018-2019
// Command handler

#pragma once
#include "MiniHDLC.h"
#include <stdint.h>

// Callback types
typedef void CmdHandlerChangeMachineFnType(const char* machineName);
typedef void CmdHandlerRxFromTargetFnType(const uint8_t* pBytes, int numBytes);
typedef void CmdHandlerPutToSerialFnType(const uint8_t* pBytes, int numBytes);

// Handles commands from ESP32
class CommandHandler
{
public:
    CommandHandler();
    ~CommandHandler();

    // Callback when machine change command is received
    void setMachineChangeCallback(CmdHandlerChangeMachineFnType* pChangeMcFunction)
    {
        _pChangeMcFunction = pChangeMcFunction;
    }

    // Callback when characters received from target machine (which goes via ESP32)
    void setRxFromTargetCallback(CmdHandlerRxFromTargetFnType* pRxFromTargetFunction)
    {
        _pRxFromTargetFunction = pRxFromTargetFunction;
    }

    // Callback when command handler wants to send on serial channel to ESP32
    void setPutToSerialCallback(CmdHandlerPutToSerialFnType* pPutToSerialFunction)
    {
        _pPutToSerialFunction = pPutToSerialFunction;
    }

    // Handle data received from ESP32 via serial connection
    static void handleSerialReceivedChars(const uint8_t* pBytes, int numBytes);

    // Service the command handler
    void service();

public:
    // Send key code to target
    void sendKeyCodeToTarget(int keyCode);
    void sendWithJSON(const char* cmdName, const char* cmdJson);
    static void sendAPIReq(const char* reqLine);
    void getStatusResponse(bool* pIPAddressValid, char** pIPAddress, char** pWifiConnStr, char** pWifiSSID);
    void requestStatusUpdate();
    void sendDebugMessage(const char* pStr);

private:
    static void static_hdlcPutCh(uint8_t ch);
    static void static_hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);
    void hdlcPutCh(uint8_t ch);
    void hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);

private:
    // Command processing
    void processCommand(const char* pCmdJson, const uint8_t* pParams, int paramsLen);
    void handleFileStart(const char* pCmdJson);
    void handleFileBlock(const char* pCmdJson, const uint8_t* pData, int dataLen);
    void handleFileEnd(const char* pCmdJson);
    void handleStatusResponse(const char* pCmdJson);
    static void addressRxCallback(uint32_t addr);

private:
    // Callbacks
    static CmdHandlerChangeMachineFnType* _pChangeMcFunction;
    static CmdHandlerRxFromTargetFnType* _pRxFromTargetFunction;
    static CmdHandlerPutToSerialFnType* _pPutToSerialFunction;

    // Singleton pointer - to allow access to the singleton bus-commander from static functions
    static CommandHandler* _pSingletonCommandHandler;

    // HDLC protocol support
    MiniHDLC _miniHDLC;

private:
    // String lengths
    static const int CMD_HANDLER_MAX_CMD_STR_LEN = 200;
    static const int MAX_INT_ARG_STR_LEN = 10;
    static const int MAX_FILE_NAME_STR = 100;
    static const int MAX_FILE_TYPE_STR = 40;

    // File name and type
    char _receivedFileName[MAX_FILE_NAME_STR+1];
    char _pReceivedFileType[MAX_FILE_TYPE_STR+1];
    char _receivedFileStartInfo[CMD_HANDLER_MAX_CMD_STR_LEN+1];

    // File position info
    uint8_t* _pReceivedFileDataPtr;
    int _receivedFileBufSize;
    int _receivedFileBytesRx;
    int _receivedBlockCount;

    // Status Command cache
    static const int MAX_ESP_HEALTH_STR = 1000;
    static const int MAX_IP_ADDR_STR = 30;
    static const int MAX_WIFI_CONN_STR = 30;
    static const int MAX_WIFI_SSID_STR = 100;
    char _statusIPAddress[MAX_IP_ADDR_STR];
    char _statusWifiConnStr[MAX_WIFI_CONN_STR];
    char _statusWifiSSID[MAX_WIFI_SSID_STR];
    bool _statusIPAddressValid;

    // Last RDP message index value string
    static const int MAX_RDP_INDEX_VAL_LEN = 20;
    char _lastRDPIndexValStr[MAX_RDP_INDEX_VAL_LEN];
    
};
