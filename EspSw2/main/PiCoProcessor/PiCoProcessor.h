/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PiCoProcessor
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>
#include "FileSystemChunker.h"
#include "MiniHDLC.h"
#include <list>

class ProtocolEndpointMsg;

class PiCoProcessor : public SysModBase
{
public:
    // Constructor/destructor
    PiCoProcessor(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, 
            ConfigBase *pMutableConfig, const char* systemVersion);
    virtual ~PiCoProcessor();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager &endpointManager) override final;

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager &endpointManager) override final;

    // Get HDLC stats
    MiniHDLCStats* getHDLCStats()
    {
        if (!_pHDLC)
            return NULL;
        return _pHDLC->getStats();
    }

    // Upload in progress
    bool uploadInProgress()
    {
        return _uploadFromAPIInProgress || _uploadFromFSInProgress;
    }

private:
    // Vars
    bool _isEnabled;

    // REST API
    RestAPIEndpointManager* _pRestAPIEndpointManager;

    // Serial details
    int _uartNum;
    int _baudRate;
    int _txPin;
    int _rxPin;
    uint32_t _rxBufSize;
    uint32_t _txBufSize;

    // Flag indicating begun
    bool _isInitialised;
    String _protocol;

    // EndpointID used to identify this message channel to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;

    // Pi status
    String _cachedPiStatusJSON;
    uint32_t _cachedPiStatusRequestMs;
    static const int TIME_BETWEEN_PI_STATUS_REQS_MS = 10000;

    // HDLC processor
    MiniHDLC* _pHDLC;
    uint32_t _hdlcMaxLen;

    // System version
    String _systemVersion;

    // Hardware version
    static const int HW_VERSION_DETECT_V20_IN_PIN = 12;
    static const int HW_VERSION_DETECT_V22_IN_PIN = 5;
    static const int HW_VERSION_DETECT_OUT_PIN = 14;
    int _hwVersion;
    static const int ESP_HW_VERSION_DEFAULT = 20;
    void detectHardwareVersion();

    // Upload of files
    bool _uploadFromFSInProgress;
    bool _uploadFromAPIInProgress;
    String _uploadFromFSRequest;
    int _uploadBlockCount;
    unsigned long _uploadStartMs;
    unsigned long _uploadLastBlockMs;
    String _uploadTargetCommandWhenComplete;
    String _uploadFileType;
    static const int MAX_UPLOAD_MS = 600000;
    static const int MAX_BETWEEN_BLOCKS_MS = 20000;
    static const int DEFAULT_BETWEEN_BLOCKS_MS = 10;
    static const int UPLOAD_BLOCK_SIZE_BYTES = 1000;
    unsigned int _uploadFilePos;
    FileSystemChunker _chunker;
    std::vector<uint8_t> _uploadBlockBuffer;
    uint32_t _fileCRC;

    // Stats
    uint32_t _statsRxCh;
    uint32_t _statsTxCh;
    uint32_t _statsRxFr;
    uint32_t _statsTxFr;
    uint32_t _statsLastReportMs;
    static const int STATS_REPORT_TIME_MS = 60000;

    // Helpers
    void applySetup();
    void sendMsgStrToPi(const char *pMsgStr);
    void sendMsgAndPayloadToPi(const uint8_t *pFrame, int frameLen);
    void sendToPi(const uint8_t *pFrame, int frameLen);
    void sendTargetCommand(const String& targetCmd, const String& reqStr);
    void sendTargetData(const String& cmdName, const uint8_t* pData, int len, int index);
    void sendResponseToPi(String& reqStr, String& msgJson);
    void hdlcFrameTxCB(const uint8_t* pFrame, int frameLen);
    void hdlcFrameRxCB(const uint8_t* pFrame, int frameLen);
    const char* getWifiStatusStr();
    void apiQueryESPHealth(const String &reqStr, String &respStr);
    void apiUploadPiSwComplete(String &reqStr, String &respStr);
    void apiUploadPiSwPart(String& req, const String& filename, size_t contentLen, size_t index, 
                const uint8_t *data, size_t len, bool finalBlock);
    void uploadAPIBlockHandler(const char* fileType, const String& req, const String& filename, 
            int fileLength, size_t index, const uint8_t *pData, size_t len, bool finalBlock);
    void apiQueryPiStatus(const String &reqStr, String &respStr);
    void apiQueryCurMc(const String &reqStr, String &respStr);
    void apiSetMcJson(const String &reqStr, String &respStr);
    void apiSetMcJsonContent(const String &reqStr, const uint8_t *pData, size_t len, size_t index, size_t total);
    void uploadCommonBlockHandler(const char* fileType, const String& req, 
            const String& filename, int fileLength, size_t index, const uint8_t *pData, size_t len, bool finalBlock);
    void sendFileStartRecord(const char* fileType, const String& req, const String& filename, int fileLength);
    void sendFileBlock(size_t index, const uint8_t *pData, size_t len);
    void sendFileEndRecord(int blockCount, const char* pAdditionalJsonNameValues);
    bool startUploadFromFileSystem(const String& fileSystemName, 
                const String& uploadRequest, const String& filename,
                const char* pTargetCmdWhenDone);
};
