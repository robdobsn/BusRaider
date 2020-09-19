// Bus Raider
// Rob Dobson 2018-2019
// Command handler

#pragma once
#include "MiniHDLC.h"
#include <stdint.h>
#include "RingBufferPosn.h"
#include "lowlib.h"
#include "CommsSocketInfo.h"

// Use acks on file transfer
#define USE_ACKS_ON_FILE_TRANSFER

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback types
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback types
typedef void CmdHandlerSerialPutBytesFnType(const uint8_t* pBytes, unsigned numBytes);
typedef uint32_t CmdHandlerSerialTxAvailableFnType();
typedef bool CmdHandlerHandleRxMsgFnType(void* pObject, const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, unsigned maxRespLen);
typedef bool CmdHandlerOTAUpdateFnType(const uint8_t* pData, unsigned dataLen);
typedef bool CmdHandlerTargetFileFnType(void* pObject, const char* rxFileInfo, const uint8_t* pData, unsigned dataLen);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles commands from ESP32
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CommandHandler
{
public:
    CommandHandler();
    ~CommandHandler();
    void init()
    {
    }

    // Comms Sockets - used to hook things like received message handling
    int commsSocketAdd(void* pSourceObject, 
                bool enabled, 
                CmdHandlerHandleRxMsgFnType* handleRxMsg, 
                CmdHandlerOTAUpdateFnType* otaUpdateFn,
                CmdHandlerTargetFileFnType* receivedFileFn);
    void commsSocketEnable(unsigned commsSocket, bool enable);

    // Callback when command handler wants to send on serial channel to ESP32
    void setPutToSerialCallback(CmdHandlerSerialPutBytesFnType* pSerialPutBytesFunction,
                    CmdHandlerSerialTxAvailableFnType* pSerialTxAvailableFunction)
    {
        _pHDLCSerialPutBytesFunction = pSerialPutBytesFunction;
        _pHDLCSerialTxAvailableFunction = pSerialTxAvailableFunction;
    }

    // Handle data received from ESP32 via serial connection
    void handleHDLCReceivedChars(const uint8_t* pBytes, unsigned numBytes);

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
    void sendKeyStrToTargetStatic(const char* pKeyStr);
    void sendKeyStrToTarget(const char* pKeyStr);

    // Num chars that can be sent
    uint32_t getTxAvailable();

    // Send
    void sendWithJSON(const char* cmdName, const char* cmdJson, uint32_t msgIdx = 0, 
            const uint8_t* pData = NULL, uint32_t dataLen = 0);
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

private:
    // Comms Sockets
    static const unsigned MAX_COMMS_SOCKETS = 20;
    CommsSocketInfo _commsSockets[MAX_COMMS_SOCKETS];
    unsigned _commsSocketCount;
    void commsSocketHandleRxMsg(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                    char* pRespJson, int maxRespLen);
    void commsSocketHandleReceivedFile(const char* fileStartInfo, uint8_t* rxData, int rxBytes, bool isFirmware);

    // HDLC
    void hdlcPutChStatic(uint8_t ch);
    uint32_t hdlcTxAvailableStatic();
    static void hdlcFrameRxStatic(const uint8_t *frameBuffer, unsigned frameLength);
    static void hdlcFrameTxStatic(const uint8_t *frameBuffer, unsigned frameLength);
    void hdlcFrameRx(const uint8_t *frameBuffer, unsigned frameLength);

    // RDP
    void sendRDPMsg(uint32_t msgIdx, const char* pCmdName, const char* pMsgJson);

    // Command processing
    void processRxCmd(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen);
    void handleFileStart(const char* pCmdJson);
    void handleFileBlock(const char* pCmdJson, const uint8_t* pData, unsigned dataLen);
    void handleFileEnd(const char* pCmdJson);
    void handleStatusResponse(const char* pCmdJson);
    void addressRxCallback(uint32_t addr);

    // File receive utils
    void fileReceiveCleardown();

    // Callbacks
    static CmdHandlerSerialPutBytesFnType* _pHDLCSerialPutBytesFunction;
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
    static const int MAX_DATAFRAME_LEN = 20000;

    // File name and type
    char _receivedFileName[MAX_FILE_NAME_STR+1];
    char _pReceivedFileType[MAX_FILE_TYPE_STR+1];
    char _receivedFileStartInfo[CMD_HANDLER_MAX_CMD_STR_LEN+1];

    // File position info
    uint8_t* _pReceivedFileDataPtr;
    unsigned _receivedFileBufSize;
    unsigned _receivedFileBytesRx;
    unsigned _receivedBlockCount;

    // Ring buffer for chars from USB
    RingBufferPosn _usbKeyboardRingBufPos;
    static const int MAX_USB_KEYBOARD_CHARS = 50;
    uint8_t _usbKeyboardRingBuffer[MAX_USB_KEYBOARD_CHARS];

    // Remote data protocol index
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

#define DEBUG_FILE_BLOCKS
#ifdef DEBUG_FILE_BLOCKS
    uint32_t _debugBlockStart[1000];
    uint32_t _debugBlockLen[1000];
    uint32_t _debugBlockRxCount;
    bool _debugBlockFirstCh;
    MiniHDLCStats _debugCurHDLCStats;
#endif
};
