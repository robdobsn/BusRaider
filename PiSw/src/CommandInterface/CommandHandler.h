// Bus Raider
// Rob Dobson 2018-2019
// Command handler

#pragma once
#include "MiniHDLC.h"
#include <stdint.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback types
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback types
typedef void CmdHandlerPutToSerialFnType(const uint8_t* pBytes, int numBytes);
typedef bool CmdHandlerHandleRxMsgFnType(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);
typedef bool CmdHandlerOTAUpdateFnType(const uint8_t* pData, int dataLen);
typedef bool CmdHandlerTargetFileFnType(const char* rxFileInfo, const uint8_t* pData, int dataLen);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms Socket Info - this is used to plug-in to the CommmandHandler layer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CommsSocketInfo
{
public:
    // Socket enablement
    bool enabled;

    // Callbacks
    CmdHandlerHandleRxMsgFnType* handleRxMsg;
    CmdHandlerOTAUpdateFnType* otaUpdateFn;
    CmdHandlerTargetFileFnType* receivedFileFn;
};

// Handles commands from ESP32
class CommandHandler
{
public:
    CommandHandler();
    ~CommandHandler();

    // Comms Sockets - used to hook things like received message handling
    static int commsSocketAdd(CommsSocketInfo& commsSocketInfo);
    static void commsSocketEnable(int commsSocket, bool enable);

    // Callback when command handler wants to send on serial channel to ESP32
    void setPutToSerialCallback(CmdHandlerPutToSerialFnType* pPutToSerialFunction)
    {
        _pPutToHDLCSerialFunction = pPutToSerialFunction;
    }

    // Handle data received from ESP32 via serial connection
    static void handleHDLCReceivedChars(const uint8_t* pBytes, int numBytes);

    // Service the command handler
    void service();

    // Get HDLC stats
    MiniHDLCStats* getHDLCStats()
    {
        return _miniHDLC.getStats();
    }

    // File transfer
    bool isFileTransferInProgress()
    {
        return _pReceivedFileDataPtr != NULL;
    }

    // Max length of machine set command
    static const int MAX_MC_SET_JSON_LEN = 10000;

    // Send key code to target
    static void sendKeyCodeToTarget(int keyCode);
    static void sendWithJSON(const char* cmdName, const char* cmdJson, uint32_t msgIdx = 0, 
            const uint8_t* pData = NULL, uint32_t dataLen = 0);
    static void sendAPIReq(const char* reqLine);
    void getStatusResponse(bool* pIPAddressValid, char** pIPAddress, char** pWifiConnStr, 
                char** pWifiSSID, char** pEsp32Version);
    void sendRegularStatusUpdate();
    void logDebugMessage(const char* pStr);
    void logDebugJson(const char* pStr);
    static void logDebug(const char* pSeverity, const char* pSource, const char* pMsg);

private:
    // Comms Sockets
    static const int MAX_COMMS_SOCKETS = 10;
    static CommsSocketInfo _commsSockets[MAX_COMMS_SOCKETS];
    static int _commsSocketCount;
    void commsSocketHandleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);
    void commsSocketHandleReceivedFile(const char* fileStartInfo, uint8_t* rxData, int rxBytes, bool isFirmware);

    // HDLC
    static void hdlcPutChStatic(uint8_t ch);
    static void hdlcFrameRxStatic(const uint8_t *frameBuffer, int frameLength);
    void hdlcPutCh(uint8_t ch);
    void hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);

    // Command processing
    void processCommand(const char* pCmdJson, const uint8_t* pParams, int paramsLen);
    void handleFileStart(const char* pCmdJson);
    void handleFileBlock(const char* pCmdJson, const uint8_t* pData, int dataLen);
    void handleFileEnd(const char* pCmdJson);
    void handleStatusResponse(const char* pCmdJson);
    static void addressRxCallback(uint32_t addr);

    // Callbacks
    static CmdHandlerPutToSerialFnType* _pPutToHDLCSerialFunction;

    // Singleton pointer - to allow access to the singleton commandHandler from static functions
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

    // // Last RDP message index value string
    // static const int MAX_RDP_INDEX_VAL_LEN = 20;
    // char _lastRDPIndexValStr[MAX_RDP_INDEX_VAL_LEN];
    
};
