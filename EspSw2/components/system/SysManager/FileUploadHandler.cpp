/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Upload Handler
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <FileUploadHandler.h>
#include <RICRESTMsg.h>
#include <FileSystem.h>
#include <ProtocolEndpointMsg.h>
#include <ProtocolEndpointManager.h>

// Log prefix
static const char *MODULE_PREFIX = "FileUploadHandler";

#define DEBUG_RICREST_FILEUPLOAD
#define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
#define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL
#define DEBUG_DUMP_UPLOAD_FIRST_BYTES
// #define DEBUG_FILE_UPLOAD_WRITE_PERF

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileUploadHandler::FileUploadHandler()
{
    // File params
    _fileSize = 0;
    _fileIsRICFirmware = false;
    _commsChannelID = 0;
    _expCRC16Valid = false;
    _expCRC16 = 0;

    // Status
    _isUploading = false;
    _fwUpdateStartPending = false;
    _fwUpdateStartedMs = 0;
    _fwUpdateStartFailed = false;
    
    // Timing
    _startMs = 0;
    _lastMsgMs = 0;

    // Stats
    _blockCount = 0;
    _bytesCount = 0;
    _blocksInWindow = 0;
    _bytesInWindow = 0;
    _statsWindowStartMs = millis();

    // Debug
    _debugLastStatsMs = millis();
    _debugFinalMsgToSend = false;

    // Batch handling
    _expectedFilePos = 0;
    _batchAckSize = BATCH_ACK_SIZE_DEFAULT;
    _blockSizeMax = FILE_BLOCK_SIZE_DEFAULT;
    _batchBlockCount = 0;
    _batchBlockAckRetry = 0;

    // Protocol endpoint manager
    _pProtocolEndpointManager = nullptr;

    // Firmware updater
    _pFirmwareUpdateSysMod = nullptr;    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle command frame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileUploadHandler::handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg)
{
    // Check file upload related
    ConfigBase cmdFrame = ricRESTReqMsg.getPayloadJson();
    if (cmdName.equalsIgnoreCase("ufStart"))
    {
        handleFileUploadStartMsg(ricRESTReqMsg.getReq(), respMsg, endpointMsg.getChannelID(), cmdFrame);
        return true;
    }
    else if (cmdName.equalsIgnoreCase("ufEnd"))
    {
        handleFileUploadEndMsg(ricRESTReqMsg.getReq(), respMsg, cmdFrame);
        return true;
    } 
    else if (cmdName.equalsIgnoreCase("ufCancel"))
    {
        handleFileUploadCancelMsg(ricRESTReqMsg.getReq(), respMsg, cmdFrame);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileUploadHandler::handleFileBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
    // Check if upload has been cancelled
    if (!_isUploading)
    {
        // Check delayed OTA start result
        if (_fwUpdateStartFailed)
        {
            // Delayed OTA start failed so report it now
            _fwUpdateStartFailed = false;
            LOG_W(MODULE_PREFIX, "handleFileBlock delayed start failed returning OTAStartFailed");
            Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, R"("cmdName":"ufStatus","reason":"OTAStartFailed")");
        }
        else
        {
            LOG_W(MODULE_PREFIX, "handleFileBlock when not uploading");
            Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, R"("cmdName":"ufStatus","reason":"notStarted")");
        }
        return false;
    }

    // Handle the upload block
    uint32_t filePos = ricRESTReqMsg.getBufferPos();
    const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
    uint32_t bufferLen = ricRESTReqMsg.getBinLen();

#ifdef DEBUG_DUMP_UPLOAD_FIRST_BYTES
    String outStr;
    static const uint32_t DEBUG_DUMP_BYTES = 5;
    Utils::getHexStrFromBytes(pBuffer, bufferLen < DEBUG_DUMP_BYTES ? bufferLen : DEBUG_DUMP_BYTES, outStr);
    LOG_I(MODULE_PREFIX, "RX %s %08x %s", _fileName.c_str(), filePos, outStr.c_str());
#endif

    // Update state machine with block
    bool blockValid = false;
    bool isFinalBlock = false;
    bool genAck = false;
    uploadBlockRx(filePos, bufferLen, blockValid, isFinalBlock, genAck);

    // Debug
    if (isFinalBlock)
    {
        LOG_I(MODULE_PREFIX, "handleFileBlock isFinal %d", isFinalBlock);
    }

    if (genAck)
    {
        // block returns true when an acknowledgement is required - so send that ack
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", _expectedFilePos);
        Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, ackJson);

#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_I(MODULE_PREFIX, "handleFileBlock BatchOK Sending OkTo %d rxBlockFilePos %d len %d batchCount %d resp %s", 
                _expectedFilePos, filePos, bufferLen, _batchBlockCount, respMsg.c_str());
#endif
    }
    else
    {
        // Check if we are actually uploading
        if (!_isUploading)
        {
            LOG_W(MODULE_PREFIX, "handleFileBlock NO UPLOAD IN PROGRESS filePos %d len %d", filePos, bufferLen);
            // Not uploading
            Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, "\"noUpload\"");
        }
        else
        {
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL
            LOG_I(MODULE_PREFIX, "handleFileBlock filePos %d len %d batchCount %d resp %s heapSpace %d", 
                    filePos, bufferLen, _batchBlockCount, respMsg.c_str(),
                    heap_caps_get_free_size(MALLOC_CAP_8BIT));
#endif
            // Just another block - don't ack yet
        }
    }

    // Check valid
    if (blockValid)
    {
        // Handle as a file block or firmware block
        if (_fileIsRICFirmware) 
        {
            // Part of system firmware
            _pFirmwareUpdateSysMod->firmwareUpdateBlock(filePos, pBuffer, bufferLen);
        }
        else
        {
#ifdef DEBUG_FILE_UPLOAD_WRITE_PERF
            uint32_t debugFileBlockWriteStartMs = millis();
#endif
            // Part of a file
            FileBlockInfo fileBlockInfo(_fileName.c_str(), 
                                _fileSize, 
                                filePos, 
                                pBuffer, 
                                bufferLen, 
                                isFinalBlock,
                                _expCRC16,
                                _expCRC16Valid,
                                _fileSize,
                                true
                                );
            fileSystem.uploadAPIBlockHandler("", _reqStr, fileBlockInfo);
#ifdef DEBUG_FILE_UPLOAD_WRITE_PERF
            LOG_I(MODULE_PREFIX, "handleFileBlock elapsed %ld", millis() - debugFileBlockWriteStartMs);
#endif
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service file upload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::service()
{
    // Check if fw update start is pending
    if (_fwUpdateStartPending && Utils::isTimeout(millis(), _fwUpdateStartedMs, FW_UPDATE_DELAY_START_BY_MS))
    {
        _fwUpdateStartPending = false;

        // Start ESP OTA update
        _fwUpdateStartFailed = true;
        if (_pFirmwareUpdateSysMod)
            _fwUpdateStartFailed = !_pFirmwareUpdateSysMod->firmwareUpdateStart(_fileName.c_str(), _fileSize);
        if (_fwUpdateStartFailed)
        {
            uploadCancel();
#ifdef DEBUG_RICREST_FILEUPLOAD
            LOG_W(MODULE_PREFIX, "serviceFileUpload firmware update start failed");
#endif
        }
        else
        {
            // Form status message
            String statusMsg;
            Utils::setJsonBoolResult("", statusMsg, true, 
                            R"("cmdName":"ufStatus","reason":"OTAStartedOK")");

            // Send status message
            RICRESTMsg ricRESTRespMsg;
            ProtocolEndpointMsg endpointMsg;
            ricRESTRespMsg.encode(statusMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
            endpointMsg.setAsResponse(_commsChannelID, MSG_PROTOCOL_RICREST, 
                        0, MSG_DIRECTION_RESPONSE);

            // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
            LOG_W(MODULE_PREFIX, "serviceFileUpload Sending OTAStartedOK");
#endif

            // Send message on the appropriate channel
            if (_pProtocolEndpointManager)
                _pProtocolEndpointManager->handleOutboundMessage(endpointMsg);            
        }
    }

#ifdef DEBUG_SHOW_FILE_UPLOAD_STATS
    // Stats display
    if (debugStatsReady())
    {
        LOG_I(MODULE_PREFIX, "fileUploadStats %s", debugStatsStr().c_str());
    }
#endif
    // Check uploading
    if (!_isUploading)
        return;
    
    // Handle upload activity
    bool genBatchAck = false;
    uploadService(genBatchAck);
    if (genBatchAck)
    {
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", _expectedFilePos);
        String statusMsg;
        Utils::setJsonBoolResult("ufBlock", statusMsg, true, ackJson);

        // Send the response back
        RICRESTMsg ricRESTRespMsg;
        ProtocolEndpointMsg endpointMsg;
        ricRESTRespMsg.encode(statusMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
        endpointMsg.setAsResponse(_commsChannelID, MSG_PROTOCOL_RICREST, 
                    0, MSG_DIRECTION_RESPONSE);

        // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
        LOG_I(MODULE_PREFIX, "serviceFileUpload Sending OkTo %d batchCount %d", 
                _expectedFilePos, _batchBlockCount);
#endif

        // Send message on the appropriate channel
        if (_pProtocolEndpointManager)
            _pProtocolEndpointManager->handleOutboundMessage(endpointMsg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get debug str
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileUploadHandler::getDebugStr()
{
    if (_pFirmwareUpdateSysMod && _pFirmwareUpdateSysMod->isBusy())
    {
        return _pFirmwareUpdateSysMod->getDebugStr();
    }
    return "";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File upload start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::handleFileUploadStartMsg(const String& reqStr, String& respMsg, uint32_t channelID, ConfigBase& cmdFrame)
{
    // Get params
    String ufStartReq = cmdFrame.getString("reqStr", "");
    uint32_t fileLen = cmdFrame.getLong("fileLen", 0);
    String fileName = cmdFrame.getString("fileName", "");
    String fileType = cmdFrame.getString("fileType", "");
    String crc16Str = cmdFrame.getString("CRC16", "");
    uint32_t crc16 = 0;
    bool crc16Valid = false;
    if (crc16Str.length() > 0)
    {
        crc16Valid = true;
        crc16 = strtoul(crc16Str.c_str(), nullptr, 0);
    }

    // Start file upload handler
    String errorMsg;
    bool startOk = uploadStart(ufStartReq, fileName, fileLen, fileType, 
                channelID, errorMsg, crc16, crc16Valid);

    // Check if firmware
    if (startOk && _fileIsRICFirmware)
    {
        // Flag indicating firmware update pending
        _fwUpdateStartPending = true;
        _fwUpdateStartedMs = millis();
    }

    // Response
    char extraJson[100];
    snprintf(extraJson, sizeof(extraJson), "\"batchMsgSize\":%d,\"batchAckSize\":%d", _blockSizeMax, _batchAckSize);
    Utils::setJsonResult(reqStr.c_str(), respMsg, startOk, errorMsg.c_str(), extraJson);

    LOG_I(MODULE_PREFIX, "handleFileUploadStartMsg reqStr %s filename %s fileType %s fileLen %d", 
                _reqStr.c_str(), _fileName.c_str(), _fileType.c_str(), _fileSize);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File upload ended normally
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::handleFileUploadEndMsg(const String& reqStr, String& respMsg, ConfigBase& cmdFrame)
{
    // Handle file end
#ifdef DEBUG_RICREST_FILEUPLOAD
    uint32_t blocksSent = cmdFrame.getLong("blockCount", 0);
#endif

    // Check if firmware
    bool rslt = true;
    if (_fileIsRICFirmware)
    {
        // Start ESP OTA update
        rslt = _pFirmwareUpdateSysMod->firmwareUpdateEnd();
    }

    // Response
    Utils::setJsonBoolResult(reqStr.c_str(), respMsg, rslt);

    // Debug
    // LOG_I(MODULE_PREFIX, "handleFileUploadEndMsg blockCount %d blockRate %f rslt %s", 
    //                 _blockCount, getBlockRate(), rslt ? "OK" : "FAIL");

    // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
    String fileName = cmdFrame.getString("fileName", "");
    String fileType = cmdFrame.getString("fileType", "");
    uint32_t fileLen = cmdFrame.getLong("fileLen", 0);
    LOG_I(MODULE_PREFIX, "handleFileUploadEndMsg reqStr %s fileName %s fileType %s fileLen %d blocksSent %d", 
                _reqStr.c_str(),
                fileName.c_str(), 
                fileType.c_str(), 
                fileLen, 
                blocksSent);
#endif

    // End upload
    uploadEnd();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cancel file upload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::handleFileUploadCancelMsg(const String& reqStr, String& respMsg, ConfigBase& cmdFrame)
{
    // Handle file cancel
    String fileName = cmdFrame.getString("fileName", "");

    // Cancel upload
    uploadCancel();

    // Response
    Utils::setJsonBoolResult(reqStr.c_str(), respMsg, true);

    // Debug
    LOG_I(MODULE_PREFIX, "handleFileUploadCancelMsg fileName %s", fileName.c_str());

    // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
    LOG_I(MODULE_PREFIX, "handleFileUploadCancelMsg reqStr %s fileName %s", 
                _reqStr.c_str(),
                fileName.c_str());
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// State machine start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileUploadHandler::uploadStart(String& reqStr, String& fileName, uint32_t fileSize, 
        String& fileType, uint32_t channelID, String& respInfo, uint32_t crc16, bool crc16Valid)
{
    // Check if already in progress
    if (_isUploading && (_expectedFilePos > 0))
    {
        respInfo = "uploadInProgress";
        return false;
    }

    // Handle block and batch sizes
    _blockSizeMax = FILE_BLOCK_SIZE_DEFAULT;
    _batchAckSize = BATCH_ACK_SIZE_DEFAULT;
    if (_pProtocolEndpointManager)
        _blockSizeMax = _pProtocolEndpointManager->getInboundBlockMax(channelID, FILE_BLOCK_SIZE_DEFAULT);
    if (_blockSizeMax * _blockSizeMax > MAX_BYTES_IN_BATCH)
    {
        _batchAckSize = MAX_BYTES_IN_BATCH / _blockSizeMax;
        if (_batchAckSize == 0)
            _batchAckSize = 1;
    }

    // File params
    _reqStr = reqStr;
    _fileName = fileName;
    _fileSize = fileSize;
    _fileType = fileType;
    _fileIsRICFirmware = (_fileType.equalsIgnoreCase("ricfw"));
    _commsChannelID = channelID;
    _expCRC16 = crc16;
    _expCRC16Valid = crc16Valid;

    // Status
    _isUploading = true;
    
    // Timing
    _startMs = millis();
    _lastMsgMs = millis();

    // Stats
    _blockCount = 0;
    _bytesCount = 0;
    _blocksInWindow = 0;
    _bytesInWindow = 0;
    _statsWindowStartMs = millis();
    _fileUploadStartMs = millis();
    _lastFileUploadTotalMs = 0;
    _lastFileUploadTotalBytes = 0;
    _lastFileUploadRateBytesPerSec = 0;

    // Debug
    _debugLastStatsMs = millis();
    _debugFinalMsgToSend = false;

    // Batch handling
    _expectedFilePos = 0;
    _batchBlockCount = 0;
    _batchBlockAckRetry = 0;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// State machine service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::uploadService(bool& genAck)
{
    unsigned long nowMillis = millis();
    genAck = false;
    // Check valid
    if (!_isUploading)
        return;
    
    // At the start of ESP firmware update there is a long delay (3s or so)
    // This occurs after reception of the first block
    // So need to ensure that there is a long enough timeout
    if (Utils::isTimeout(nowMillis, _lastMsgMs, _blockCount < 2 ? FIRST_MSG_TIMEOUT : BLOCK_MSGS_TIMEOUT))
    {
        _batchBlockAckRetry++;
        if (_batchBlockAckRetry < MAX_BATCH_BLOCK_ACK_RETRIES)
        {
            LOG_W(MODULE_PREFIX, "uploadService blockMsgs timeOut - okto ack needed bytesRx %d lastOkTo %d lastMsgMs %d curMs %ld blkCount %d blkSize %d batchSize %d retryCount %d",
                        _bytesCount, _expectedFilePos, _lastMsgMs, nowMillis, _blockCount, _blockSizeMax, _batchAckSize, _batchBlockAckRetry);
            _lastMsgMs = nowMillis;
            genAck = true;
            return;
        }
        else
        {
            LOG_W(MODULE_PREFIX, "uploadService blockMsgs ack failed after retries");
            uploadCancel("failRetries");
        }
    }

    // Check for overall time-out
    if (Utils::isTimeout(nowMillis, _startMs, UPLOAD_FAIL_TIMEOUT_MS))
    {
        LOG_W(MODULE_PREFIX, "uploadService overall time-out startMs %d nowMs %ld maxMs %d",
                    _startMs, nowMillis, UPLOAD_FAIL_TIMEOUT_MS);
        uploadCancel("failTimeout");
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle received block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::uploadBlockRx(uint32_t filePos, uint32_t blockLen, bool& blockValid, bool& isFinalBlock, bool& genAck)
{
    // Check uploading
    if (!_isUploading)
        return;

    // Returned vals
    blockValid = false;
    isFinalBlock = false;
    genAck = false;

    // Add to batch
    _batchBlockCount++;
    _lastMsgMs = millis();

    // Check
    if (filePos != _expectedFilePos)
    {
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_W(MODULE_PREFIX, "uploadBlockRx unexpected %d != %d, blockCount %d batchBlockCount %d, batchAckSize %d", 
                    filePos, _expectedFilePos, _blockCount, _batchBlockCount, _batchAckSize);
#endif
    }
    else
    {
        // Valid block so bump values
        blockValid = true;
        _expectedFilePos += blockLen;
        _blockCount++;
        _bytesCount += blockLen;
        _blocksInWindow++;
        _bytesInWindow += blockLen;

        // Check if this is the final block
        isFinalBlock = checkFinalBlock(filePos, blockLen);

#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_I(MODULE_PREFIX, "uploadBlockRx ok blockCount %d batchBlockCount %d, batchAckSize %d",
                    _blockCount, _batchBlockCount, _batchAckSize);
#endif
    }

    // Generate an ack on the first block received and on completion of each batch
    bool batchComplete = (_batchBlockCount == _batchAckSize) || (_blockCount == 1) || isFinalBlock;
    if (batchComplete)
        _batchBlockCount = 0;
    _batchBlockAckRetry = 0;
    genAck = batchComplete;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// State machine upload cancel
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::uploadCancel(const char* reasonStr)
{
    _isUploading = false;
    if (_pFirmwareUpdateSysMod)
        _pFirmwareUpdateSysMod->firmwareUpdateCancel();

    // Check if we need to send back a reason
    if (reasonStr != nullptr)
    {
        // Form cancel message
        String cancelMsg;
        char tmpStr[50];
        snprintf(tmpStr, sizeof(tmpStr), R"("cmdName":"ufCancel","reason":"%s")", reasonStr);
        Utils::setJsonBoolResult("", cancelMsg, true, tmpStr);

        // Send status message
        RICRESTMsg ricRESTRespMsg;
        ProtocolEndpointMsg endpointMsg;
        ricRESTRespMsg.encode(cancelMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
        endpointMsg.setAsResponse(_commsChannelID, MSG_PROTOCOL_RICREST, 
                    0, MSG_DIRECTION_RESPONSE);

        // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
        LOG_W(MODULE_PREFIX, "uploadCancel ufCancel reason %s", reasonStr);
#endif

        // Send message on the appropriate channel
        if (_pProtocolEndpointManager)
            _pProtocolEndpointManager->handleOutboundMessage(endpointMsg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload end
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileUploadHandler::uploadEnd()
{
    _isUploading = false;
    _debugFinalMsgToSend = true;
    _lastFileUploadTotalMs = millis() - _fileUploadStartMs;
    if (_lastFileUploadTotalMs != 0) {
        _lastFileUploadRateBytesPerSec = (1000.0 * _lastFileUploadTotalBytes) / _lastFileUploadTotalMs;
    } else {
        _lastFileUploadRateBytesPerSec = 0;
    }
}
