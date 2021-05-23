/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Upload Handler
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <WString.h>
#include <Logger.h>
#include <ConfigBase.h>
#include <Utils.h>
#include "ArduinoTime.h"

class RICRESTMsg;
class ProtocolEndpointMsg;
class ProtocolEndpointManager;
class SysModBase;

class FileUploadHandler
{
public:
    static const uint32_t FIRST_MSG_TIMEOUT = 3000;
    static const uint32_t BLOCK_MSGS_TIMEOUT = 1000;
    static const uint32_t MAX_BATCH_BLOCK_ACK_RETRIES = 5;
    static const uint32_t FILE_BLOCK_SIZE_DEFAULT = 220;
    static const uint32_t BATCH_ACK_SIZE_DEFAULT = 5;
    static const uint32_t FW_UPDATE_DELAY_START_BY_MS = 1000;
    static const uint32_t MAX_BYTES_IN_BATCH = 50000;
    // The overall timeout needs to be very big as BLE uploads can take over 30 minutes
    static const uint32_t UPLOAD_FAIL_TIMEOUT_MS = 2 * 3600 * 1000;

    // Constructor
    FileUploadHandler();

    // Service file upload
    void service();

    // Handle command frame
    bool handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg);

    // Handle received file block
    bool handleFileBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg);

    // Setup
    void setup(ProtocolEndpointManager* pProtocolEndpointManager)
    {
        _pProtocolEndpointManager = pProtocolEndpointManager;
    }

    // Set firmware updater
    void setFWUpdater(SysModBase* pFirmwareUpdateSysMod)
    {
        LOG_I("FWUploadMan", "setFWUpdater %d", pFirmwareUpdateSysMod ? 1 : 0);
        _pFirmwareUpdateSysMod = pFirmwareUpdateSysMod;
    }

    // Get debug str
    String getDebugStr();
    
private:

    double getBlockRate()
    {
        uint32_t elapsedMs = millis() - _startMs;
        if (elapsedMs > 0)
            return 1000.0*_blockCount/elapsedMs;
        return 0;
    }
    bool checkFinalBlock(uint32_t filePos, uint32_t blockLen)
    {
        return filePos + blockLen >= _fileSize;
    }

    bool debugStatsReady()
    {
        return _debugFinalMsgToSend || (_isUploading && Utils::isTimeout(millis(), _debugLastStatsMs, DEBUG_STATS_MS));
    }

    String debugStatsStr()
    {
        char outStr[200];
        if (_debugFinalMsgToSend)
        {
            snprintf(outStr, sizeof(outStr), "Complete OverallMsgRate %.1f/s OverallDataRate %.1f Bytes/s, TotalMsgs %d, TotalBytes %d, TotalMs %d, EndToEndRate %.1f Bytes/s",
                    statsFinalMsgRate(), statsFinalDataRate(), 
                    _blockCount, _bytesCount, 
                    _lastFileUploadTotalMs,
                    _lastFileUploadRateBytesPerSec);
        }
        else
        {
            snprintf(outStr, sizeof(outStr), "MsgRate %.1f/s DataRate %.1f Bytes/s, MsgCount %d, BytesCount %d",
                    statsMsgRate(), statsDataRate(), _blockCount, _bytesCount);
        }
        statsEndWindow();
        _debugLastStatsMs = millis();
        _debugFinalMsgToSend = false;
        return outStr;
    }

    double statsMsgRate()
    {
        uint32_t winMs = millis() - _statsWindowStartMs;
        if (winMs == 0)
            return 0;
        return 1000.0 * _blocksInWindow / winMs;
    }

    double statsDataRate()
    {
        uint32_t winMs = millis() - _statsWindowStartMs;
        if (winMs == 0)
            return 0;
        return 1000.0 * _bytesInWindow / winMs;
    }

    double statsFinalMsgRate()
    {
        uint32_t winMs = _lastMsgMs - _startMs;
        if (winMs == 0)
            return 0;
        return 1000.0 * _blockCount / winMs;
    }

    double statsFinalDataRate()
    {
        uint32_t winMs = _lastMsgMs - _startMs;
        if (winMs == 0)
            return 0;
        return 1000.0 * _bytesCount / winMs;
    }

    void statsEndWindow()
    {
        _blocksInWindow = 0;
        _bytesInWindow = 0;
        _statsWindowStartMs = millis();
    }

    // File info
    String _reqStr;
    uint32_t _fileSize;
    String _fileName;
    String _fileType;
    bool _fileIsRICFirmware;
    uint32_t _expCRC16;
    bool _expCRC16Valid;

    // Upload state
    bool _isUploading;
    uint32_t _startMs;
    uint32_t _lastMsgMs;
    uint32_t _blockSizeMax;
    uint32_t _batchAckSize;
    uint32_t _commsChannelID;

    // Delayed start to firmware update
    bool _fwUpdateStartPending;
    uint32_t _fwUpdateStartedMs;
    bool _fwUpdateStartFailed;

    // Stats
    uint32_t _blockCount;
    uint32_t _bytesCount;
    uint32_t _blocksInWindow;
    uint32_t _bytesInWindow;
    uint32_t _statsWindowStartMs;
    uint32_t _fileUploadStartMs;
    uint32_t _lastFileUploadTotalMs;
    uint32_t _lastFileUploadTotalBytes;
    float _lastFileUploadRateBytesPerSec;

    // Batch handling
    uint32_t _expectedFilePos;
    uint32_t _batchBlockCount;
    uint32_t _batchBlockAckRetry;

    // Firmware update sysMod
    SysModBase* _pFirmwareUpdateSysMod;

    // Protocol endpoints
    ProtocolEndpointManager* _pProtocolEndpointManager;

    // Debug
    uint32_t _debugLastStatsMs;
    static const uint32_t DEBUG_STATS_MS = 1000;
    bool _debugFinalMsgToSend;

    // Message helpers
    void handleFileUploadStartMsg(const String& reqStr, String& respMsg, uint32_t channelID, ConfigBase& cmdFrame);
    void handleFileUploadEndMsg(const String& reqStr, String& respMsg, ConfigBase& cmdFrame);
    void handleFileUploadCancelMsg(const String& reqStr, String& respMsg, ConfigBase& cmdFrame);

    // Upload state-machine helpers
    void uploadService(bool& genAck);
    // Start file upload
    bool uploadStart(String& reqStr, String& fileName, uint32_t fileSize, 
            String& fileType, uint32_t channelID, String& respInfo, uint32_t crc16, bool crc16Valid);
    void uploadBlockRx(uint32_t filePos, uint32_t blockLen, bool& blockValid, bool& isFinalBlock, bool& genAck);
    void uploadCancel(const char* reasonStr = nullptr);
    void uploadEnd();
};