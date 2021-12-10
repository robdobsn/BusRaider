/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Upload Protocol
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <vector>
#include <functional>
#include <WString.h>
#include <Logger.h>
#include <JSONParams.h>
#include <Utils.h>
#include "ArduinoTime.h"
#include "FileStreamBlock.h"

// Received block callback function type
typedef std::function<bool(FileStreamBlock& fileBlock)> FileUploadBlockCBFnType;
typedef std::function<void(bool isNormalEnd)> FileUploadCancelEndCBFnType;

class RICRESTMsg;
class ProtocolEndpointMsg;
class ProtocolEndpointManager;
class SysModBase;

class ProtocolFileStream
{
public:
    // StreamID values
    static const uint32_t FILE_STREAM_ID_ANY = 0;
    static const uint32_t FILE_STREAM_ID_MIN = 1;
    static const uint32_t FILE_STREAM_ID_MAX = 255;

    // Consts
    static const uint32_t FIRST_MSG_TIMEOUT = 5000;
    static const uint32_t BLOCK_MSGS_TIMEOUT = 1000;
    static const uint32_t MAX_BATCH_BLOCK_ACK_RETRIES = 5;
    static const uint32_t FILE_BLOCK_SIZE_DEFAULT = 5000;
    static const uint32_t BATCH_ACK_SIZE_DEFAULT = 40;
    static const uint32_t MAX_TOTAL_BYTES_IN_BATCH = 50000;
    // The overall timeout needs to be very big as BLE uploads can take over 30 minutes
    static const uint32_t UPLOAD_FAIL_TIMEOUT_MS = 2 * 3600 * 1000;

    // File/Stream Type
    enum FileStreamType
    {
        FILE_STREAM_TYPE_FILE,
        FILE_STREAM_TYPE_FIRMWARE,
        FILE_STREAM_TYPE_RT_STREAM,
        FILE_STREAM_TYPE_RELIABLE_STREAM
    };

    // Constructor
    ProtocolFileStream(FileUploadBlockCBFnType fileRxBlockCB, 
            FileUploadCancelEndCBFnType fileRxCancelCB,
            ProtocolEndpointManager* pProtocolEndpointManager,
            ProtocolFileStream::FileStreamType fileStreamType, uint32_t streamID);

    // Service file upload
    void service();

    // Handle command frame
    bool handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg);

    // Handle received file/stream block
    bool handleFileStreamBlockMsg(RICRESTMsg& ricRESTReqMsg, String& respMsg);

    // Get debug str
    String getDebugJSON(bool includeBraces);

    // Check if message is for file/streaming
    enum FileStreamMsgType
    {
        FILE_STREAM_MSG_TYPE_NONE,
        FILE_STREAM_MSG_TYPE_START,
        FILE_STREAM_MSG_TYPE_END,
        FILE_STREAM_MSG_TYPE_CANCEL
    };
    static FileStreamMsgType getFileStreamMsgType(const RICRESTMsg& ricRESTReqMsg,
                String& cmdName, String& fileStreamName, FileStreamType& fileStreamType, uint32_t& streamID);

    // Get fileStreamType from string
    static FileStreamType getFileStreamType(const String& fileStreamTypeStr)
    {
        if ((fileStreamTypeStr.length() == 0) || fileStreamTypeStr.equalsIgnoreCase("fs") || fileStreamTypeStr.equalsIgnoreCase("file"))
            return FILE_STREAM_TYPE_FILE;
        if (fileStreamTypeStr.equalsIgnoreCase("fw") || fileStreamTypeStr.equalsIgnoreCase("ricfw"))
            return FILE_STREAM_TYPE_FIRMWARE;
        if (fileStreamTypeStr.equalsIgnoreCase("rtstream"))
            return FILE_STREAM_TYPE_RT_STREAM;
        return FILE_STREAM_TYPE_RELIABLE_STREAM;
    }

    // Get streamID
    uint32_t getStreamID()
    {
        return _streamID;
    }

    // Is busy
    bool isBusy()
    {
        return _isUploading;
    }

private:
    // Callbacks
    FileUploadBlockCBFnType _fileRxBlockCB;
    FileUploadCancelEndCBFnType _fileRxCancelEndCB;

    // Message helpers
    void handleFileUploadStartMsg(const String& reqStr, String& respMsg, uint32_t channelID, const JSONParams& cmdFrame);
    void handleFileUploadEndMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame);
    void handleFileUploadCancelMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame);

    // Upload state-machine helpers
    void uploadService(bool& genAck);
    bool validateUploadStart(String& reqStr, String& fileName, uint32_t fileSize, 
            uint32_t channelID, String& respInfo, uint32_t crc16, bool crc16Valid);
    void validateRxBlock(uint32_t filePos, uint32_t blockLen, 
                bool& isFirstBlock, bool& blockValid, bool& isFinalBlock, bool& genAck);
    void uploadCancel(const char* reasonStr = nullptr);
    void uploadEnd();

    // Helpers
    uint32_t getOkTo();
    double getBlockRate();
    bool checkFinalBlock(uint32_t filePos, uint32_t blockLen);
    bool debugStatsReady();
    String debugStatsStr();
    double statsMsgRate();
    double statsDataRate();
    double statsFinalMsgRate();
    double statsFinalDataRate();
    void statsEndWindow();

    // Protocol endpoints
    ProtocolEndpointManager* _pProtocolEndpointManager;

    // File info
    String _reqStr;
    uint32_t _fileSize;
    String _fileName;
    FileStreamType _fileStreamType;
    uint32_t _expCRC16;
    bool _expCRC16Valid;

    // Upload state
    bool _isUploading;
    uint32_t _startMs;
    uint32_t _lastMsgMs;
    uint32_t _commsChannelID;

    // Size of batch and block
    uint32_t _batchAckSize;
    uint32_t _blockSize;

    // Stats
    uint32_t _blockCount;
    uint32_t _bytesCount;
    uint32_t _blocksInWindow;
    uint32_t _bytesInWindow;
    uint32_t _statsWindowStartMs;
    uint32_t _fileUploadStartMs;

    // Batch handling
    uint32_t _expectedFilePos;
    uint32_t _batchBlockCount;
    uint32_t _batchBlockAckRetry;

    // StreamID
    uint32_t _streamID;

    // Debug
    uint32_t _debugLastStatsMs;
    static const uint32_t DEBUG_STATS_MS = 10000;
    bool _debugFinalMsgToSend;
};