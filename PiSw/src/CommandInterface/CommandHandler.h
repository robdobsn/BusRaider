// Bus Raider
// Rob Dobson 2018-2019
// Command handler

#pragma once
#include "MiniHDLC.h"
#include <stdint.h>
#include "../System/RingBufferPosn.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback types
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback types
typedef void CmdHandlerSerialPutStrFnType(const uint8_t* pBytes, int numBytes);
typedef uint32_t CmdHandlerSerialTxAvailableFnType();
typedef bool CmdHandlerHandleRxMsgFnType(void* pObject, const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);
typedef bool CmdHandlerOTAUpdateFnType(const uint8_t* pData, int dataLen);
typedef bool CmdHandlerTargetFileFnType(void* pObject, const char* rxFileInfo, const uint8_t* pData, int dataLen);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms Socket Info - this is used to plug-in to the CommmandHandler layer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handles commands from ESP32
class CommandHandler
{
public:
    static const int MAX_DATAFRAME_LEN = 5000;

    CommandHandler();
    ~CommandHandler();

    // Comms Sockets - used to hook things like received message handling
    int commsSocketAdd(void* pSourceObject, 
                bool enabled, 
                CmdHandlerHandleRxMsgFnType* handleRxMsg, 
                CmdHandlerOTAUpdateFnType* otaUpdateFn,
                CmdHandlerTargetFileFnType* receivedFileFn);
    void commsSocketEnable(int commsSocket, bool enable);

    // Callback when command handler wants to send on serial channel to ESP32
    void setPutToSerialCallback(CmdHandlerSerialPutStrFnType* pSerialPutStrFunction,
                    CmdHandlerSerialTxAvailableFnType* pSerialTxAvailableFunction)
    {
        _pHDLCSerialPutStrFunction = pSerialPutStrFunction;
        _pHDLCSerialTxAvailableFunction = pSerialTxAvailableFunction;
    }

    // Handle data received from ESP32 via serial connection
    void handleHDLCReceivedChars(const uint8_t* pBytes, int numBytes);

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
        return _pSingletonCommandHandler->_pReceivedFileDataPtr != NULL;
    }

    // Max length of machine set command
    static const int MAX_MC_SET_JSON_LEN = 10000;

    // Keys passed in keyHandler
    static const int NUM_USB_KEYS_PASSED = 6;

    // Send key code to target
    static void sendKeyStrToTargetStatic(const char* pKeyStr);
    void sendKeyStrToTarget(const char* pKeyStr);

    // Num chars that can be sent
    uint32_t getTxAvailable();

    // Send
    void sendWithJSON(const char* cmdName, const char* cmdJson, uint32_t msgIdx = 0, 
            const uint8_t* pData = NULL, uint32_t dataLen = 0);
    // static void sendWithJSONTest(const char* cmdName, const char* cmdJson, uint32_t msgIdx = 0, 
    //         const uint8_t* pData = NULL, uint32_t dataLen = 0);
    void sendAPIReq(const char* reqLine);
    // Send unnumbered message
    void sendUnnumberedMsg(const char* pCmdName, const char* pMsgJson);

    // Get status
    void getStatusResponse(bool* pIPAddressValid, char** pIPAddress, char** pWifiConnStr, 
                char** pWifiSSID, char** pEsp32Version);
    void sendRegularStatusUpdate();

    // Logging
    void logDebugMessage(const char* pStr);
    void logDebugJson(const char* pStr);
    void logDebug(const char* pSeverity, const char* pSource, const char* pMsg);

    // File Receive Status
    bool getFileReceiveStatus(uint32_t& fileLen, uint32_t& filePos);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Comms Socket Info - this is used to plug-in to the CommmandHandler layer
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class CommsSocketInfo
    {
    public:
        void set(void* pSourceObject,
                    bool enabled, CmdHandlerHandleRxMsgFnType* handleRxMsg, 
                    CmdHandlerOTAUpdateFnType* otaUpdateFn,
                    CmdHandlerTargetFileFnType* receivedFileFn)
        {
            this->pSourceObject = pSourceObject;
            this->enabled = enabled;
            this->handleRxMsg = handleRxMsg;
            this->otaUpdateFn = otaUpdateFn;
            this->receivedFileFn = receivedFileFn;
        }

        // Source object
        void* pSourceObject;

        // Socket enablement
        bool enabled;

        // Callbacks
        CmdHandlerHandleRxMsgFnType* handleRxMsg;
        CmdHandlerOTAUpdateFnType* otaUpdateFn;
        CmdHandlerTargetFileFnType* receivedFileFn;
    };

private:
    // Comms Sockets
    static const int MAX_COMMS_SOCKETS = 10;
    CommsSocketInfo _commsSockets[MAX_COMMS_SOCKETS];
    int _commsSocketCount;
    void commsSocketHandleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen);
    void commsSocketHandleReceivedFile(const char* fileStartInfo, uint8_t* rxData, int rxBytes, bool isFirmware);

    // HDLC
    static void hdlcPutChStatic(uint8_t ch);
    uint32_t hdlcTxAvailableStatic();
    static void hdlcFrameRxStatic(const uint8_t *frameBuffer, int frameLength);
    void hdlcPutCh(uint8_t ch);
    void hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);

    // RDP
    void sendRDPMsg(uint32_t msgIdx, const char* pCmdName, const char* pMsgJson);

    // Command processing
    void processCommand(const char* pCmdJson, const uint8_t* pParams, int paramsLen);
    void handleFileStart(const char* pCmdJson);
    void handleFileBlock(const char* pCmdJson, const uint8_t* pData, int dataLen);
    void handleFileEnd(const char* pCmdJson);
    void handleStatusResponse(const char* pCmdJson);
    static void addressRxCallback(uint32_t addr);

    // File receive utils
    void fileReceiveCleardown();

    // Callbacks
    static CmdHandlerSerialPutStrFnType* _pHDLCSerialPutStrFunction;
    static CmdHandlerSerialTxAvailableFnType* _pHDLCSerialTxAvailableFunction;

    // Singleton pointer - to allow access to the singleton commandHandler from static functions
    static CommandHandler* _pSingletonCommandHandler;

    // HDLC protocol support
    MiniHDLC _miniHDLC;

private:
    // String lengths
    static const int CMD_HANDLER_MAX_CMD_STR_LEN = 2000;
    static const int MAX_INT_ARG_STR_LEN = 20;
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

    // Ring buffer for chars from USB
    RingBufferPosn _usbKeyboardRingBufPos;
    static const int MAX_USB_KEYBOARD_CHARS = 50;
    uint8_t _usbKeyboardRingBuffer[MAX_USB_KEYBOARD_CHARS];

    static const uint32_t RDP_INDEX_NONE = 0xffffffff;

    // Timeouts
    uint32_t _receivedFileLastBlockMs;
    static const uint32_t MAX_FILE_RECEIVE_BETWEEN_BLOCKS_MS = 3000;

    // // Last RDP message index value string
    // static const int MAX_RDP_INDEX_VAL_LEN = 20;
    // char _lastRDPIndexValStr[MAX_RDP_INDEX_VAL_LEN];

    // TODO
    // int _rdpMsgCountIn;
    // int _rdpMsgCountOut;
    // uint32_t _rdpTimeUs; 
};
