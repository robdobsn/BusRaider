/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Upload Protocol
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <JSONParams.h>
#include <ProtocolFileStream.h>
#include <RICRESTMsg.h>
#include <FileSystem.h>
#include <ProtocolEndpointMsg.h>
#include <ProtocolEndpointManager.h>

// Log prefix
static const char *MODULE_PREFIX = "ProtocolFileStream";

// #define DEBUG_SHOW_FILE_UPLOAD_PROGRESS
// #define DEBUG_RICREST_FILEUPLOAD
// #define DEBUG_RICREST_FILEUPLOAD_FIRST_AND_LAST_BLOCK
// #define DEBUG_FILE_STREAM_BLOCK_DETAIL
// #define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
// #define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolFileStream::ProtocolFileStream(FileUploadBlockCBFnType fileRxBlockCB, 
            FileUploadCancelEndCBFnType fileRxCancelEndCB,
            ProtocolEndpointManager* pProtocolEndpointManager,
            ProtocolFileStream::FileStreamType fileStreamType, uint32_t streamID)
{
    // File params
    _fileRxBlockCB = fileRxBlockCB;
    _fileRxCancelEndCB = fileRxCancelEndCB;
    _fileSize = 0;
    _fileStreamType = fileStreamType;
    _commsChannelID = 0;
    _expCRC16Valid = false;
    _expCRC16 = 0;
    _pProtocolEndpointManager = pProtocolEndpointManager;

    // Status
    _isUploading = false;

    // Timing
    _startMs = 0;
    _lastMsgMs = 0;

    // Stats
    _blockCount = 0;
    _bytesCount = 0;
    _blocksInWindow = 0;
    _bytesInWindow = 0;
    _statsWindowStartMs = millis();
    _fileUploadStartMs = 0;

    // Debug
    _debugLastStatsMs = millis();
    _debugFinalMsgToSend = false;

    // Batch and block size
    _batchAckSize = BATCH_ACK_SIZE_DEFAULT;
    _blockSize = FILE_BLOCK_SIZE_DEFAULT;

    // Batch handling
    _expectedFilePos = 0;
    _batchBlockCount = 0;
    _batchBlockAckRetry = 0;

    // Stream
    _streamID = streamID;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Check if message is for file/streaming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolFileStream::FileStreamMsgType ProtocolFileStream::getFileStreamMsgType(const RICRESTMsg& ricRESTReqMsg,
                String& cmdName, String& fileStreamName, FileStreamType& fileStreamType, uint32_t& streamID)
{
    // Handle command frames
    JSONParams cmdFrame = ricRESTReqMsg.getPayloadJson();
    cmdName = cmdFrame.getString("cmdName", "");

    // Check msg type
    FileStreamMsgType fileStreamMsgType = FILE_STREAM_MSG_TYPE_NONE;
    if (cmdName.equalsIgnoreCase("ufStart"))
        fileStreamMsgType = FILE_STREAM_MSG_TYPE_START;
    if (cmdName.equalsIgnoreCase("ufEnd"))
        fileStreamMsgType = FILE_STREAM_MSG_TYPE_END;
    if (cmdName.equalsIgnoreCase("ufCancel"))
        fileStreamMsgType = FILE_STREAM_MSG_TYPE_CANCEL;

    // Return none if not a file/stream protocol message
    if (fileStreamMsgType == FILE_STREAM_MSG_TYPE_NONE)
        return fileStreamMsgType;

    // Extract info
    fileStreamName = cmdFrame.getString("fileName", "");
    String fileStreamTypeStr = cmdFrame.getString("fileType", "");
    streamID = cmdFrame.getLong("streamID", FILE_STREAM_ID_ANY);
    fileStreamType = getFileStreamType(fileStreamTypeStr);
    return fileStreamMsgType;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle command frame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolFileStream::handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg)
{
    // Check file upload related
    JSONParams cmdFrame = ricRESTReqMsg.getPayloadJson();
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
// Handle file/stream block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolFileStream::handleFileStreamBlockMsg(RICRESTMsg& ricRESTReqMsg, String& respMsg)
{
#ifdef DEBUG_FILE_STREAM_BLOCK_DETAIL
    LOG_I(MODULE_PREFIX, "handleFileStreamBlockMsg isUploading %d msgLen %d expectedPos %d", 
            _isUploading, ricRESTReqMsg.getBinLen(), _expectedFilePos);
#endif

    // Check if upload has been cancelled
    if (!_isUploading)
    {
        LOG_W(MODULE_PREFIX, "handleFileBlock when not uploading");
        uploadCancel("failBlockUnexpected");
        return false;
    }

    // Handle the upload block
    uint32_t filePos = ricRESTReqMsg.getBufferPos();
    const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
    uint32_t bufferLen = ricRESTReqMsg.getBinLen();

    // Validate received block and update state machine
    bool blockValid = false;
    bool isFinalBlock = false;
    bool isFirstBlock = false;
    bool genAck = false;
    validateRxBlock(filePos, bufferLen, blockValid, isFirstBlock, isFinalBlock, genAck);

    // Debug
    if (isFinalBlock)
    {
        LOG_I(MODULE_PREFIX, "handleFileBlock isFinal %d", isFinalBlock);
    }

#ifdef DEBUG_RICREST_FILEUPLOAD_FIRST_AND_LAST_BLOCK
    if (isFinalBlock || (_expectedFilePos == 0))
    {
        String hexStr;
        Utils::getHexStrFromBytes(ricRESTReqMsg.getBinBuf(), ricRESTReqMsg.getBinLen(), hexStr);
        LOG_I(MODULE_PREFIX, "handleFileBlock %s", hexStr.c_str());
    }
#endif

    // Check if time to generate an ACK
    if (genAck)
    {
        // block returns true when an acknowledgement is required - so send that ack
        char ackJson[100];
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", getOkTo());
        Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true, ackJson);

#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_I(MODULE_PREFIX, "handleFileBlock BatchOK Sending OkTo %d rxBlockFilePos %d len %d batchCount %d resp %s", 
                getOkTo(), filePos, bufferLen, _batchBlockCount, respMsg.c_str());
#endif
    }
    else
    {
        // Just another block - don't ack yet
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL
        LOG_I(MODULE_PREFIX, "handleFileBlock filePos %d len %d batchCount %d resp %s heapSpace %d", 
                filePos, bufferLen, _batchBlockCount, respMsg.c_str(),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#endif
    }

    // Check valid
    bool rsltOk = true;
    if (blockValid && _fileRxBlockCB)
    {
        FileStreamBlock fileStreamBlock(_fileName.c_str(), 
                            _fileSize, 
                            filePos, 
                            pBuffer, 
                            bufferLen, 
                            isFinalBlock,
                            _expCRC16,
                            _expCRC16Valid,
                            _fileSize,
                            true,
                            isFirstBlock
                            );            

        // If this is the first block of a firmware update then there will be a long delay
        rsltOk = _fileRxBlockCB(fileStreamBlock);

        // Check result
        if (!rsltOk)
        {
            if (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_FIRMWARE)
            {
                Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, R"("cmdName":"ufStatus","reason":"OTAWriteFailed")");
                if (isFirstBlock)
                    uploadCancel("failOTAStart");
                else
                    uploadCancel("failOTAWrite");
            }
            else
            {
                Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, false, R"("cmdName":"ufStatus","reason":"FileWriteFailed")");
                uploadCancel("failFileWrite");
            }
        }
    }
    return rsltOk;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service file upload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolFileStream::service()
{
#ifdef DEBUG_SHOW_FILE_UPLOAD_PROGRESS
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
        snprintf(ackJson, sizeof(ackJson), "\"okto\":%d", getOkTo());
        String respMsg;
        Utils::setJsonBoolResult("ufBlock", respMsg, true, ackJson);

        // Send the response back
        RICRESTMsg ricRESTRespMsg;
        ProtocolEndpointMsg endpointMsg;
        ricRESTRespMsg.encode(respMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
        endpointMsg.setAsResponse(_commsChannelID, MSG_PROTOCOL_RICREST, 
                    0, MSG_TYPE_RESPONSE);

        // Debug
#ifdef DEBUG_RICREST_FILEUPLOAD
        LOG_I(MODULE_PREFIX, "service Sending OkTo %d batchCount %d", 
                getOkTo(), _batchBlockCount);
#endif

        // Send message on the appropriate channel
        if (_pProtocolEndpointManager)
            _pProtocolEndpointManager->handleOutboundMessage(endpointMsg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get debug str
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ProtocolFileStream::getDebugJSON(bool includeBraces)
{
    if (includeBraces)
        return "{" + debugStatsStr() + "}";
    return debugStatsStr();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File upload start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolFileStream::handleFileUploadStartMsg(const String& reqStr, String& respMsg, uint32_t channelID, const JSONParams& cmdFrame)
{
    // Get params
    String ufStartReq = cmdFrame.getString("reqStr", "");
    uint32_t fileLen = cmdFrame.getLong("fileLen", 0);
    String fileName = cmdFrame.getString("fileName", "");
    String fileType = cmdFrame.getString("fileType", "");
    String crc16Str = cmdFrame.getString("CRC16", "");
    uint32_t blockSizeFromHost = cmdFrame.getLong("batchMsgSize", -1);
    uint32_t batchAckSizeFromHost = cmdFrame.getLong("batchAckSize", -1);
    // CRC handling
    uint32_t crc16 = 0;
    bool crc16Valid = false;
    if (crc16Str.length() > 0)
    {
        crc16Valid = true;
        crc16 = strtoul(crc16Str.c_str(), nullptr, 0);
    }

    // Validate the start message
    String errorMsg;
    bool startOk = validateUploadStart(ufStartReq, fileName, fileLen,  
                channelID, errorMsg, crc16, crc16Valid);

    // Check ok
    if (startOk)
    {
        // If we are sent batchMsgSize and/or batchAckSize then we use these values
        if ((blockSizeFromHost != -1) && (blockSizeFromHost > 0))
            _blockSize = blockSizeFromHost;
        else
            _blockSize = FILE_BLOCK_SIZE_DEFAULT;
        if ((batchAckSizeFromHost != -1) && (batchAckSizeFromHost > 0))
            _batchAckSize = batchAckSizeFromHost;
        else
            _batchAckSize = BATCH_ACK_SIZE_DEFAULT;

        // Check block sizes against channel maximum
        if (_pProtocolEndpointManager)
        {
            uint32_t chanBlockMax = _pProtocolEndpointManager->getInboundBlockMax(channelID, FILE_BLOCK_SIZE_DEFAULT);
            if ((_blockSize > chanBlockMax) && (chanBlockMax > 0))
                _blockSize = chanBlockMax;
        }

        // Check maximum total bytes in batch
        uint32_t totalBytesInBatch = _blockSize * _batchAckSize;
        if (totalBytesInBatch > MAX_TOTAL_BYTES_IN_BATCH)
        {
            _batchAckSize = MAX_TOTAL_BYTES_IN_BATCH / _blockSize;
            if (_batchAckSize == 0)
                _batchAckSize = 1;
        }
    }
    // Response
    char extraJson[100];
    snprintf(extraJson, sizeof(extraJson), R"("batchMsgSize":%d,"batchAckSize":%d,"streamID":%d)", 
                _blockSize, _batchAckSize, _streamID);
    Utils::setJsonResult(reqStr.c_str(), respMsg, startOk, errorMsg.c_str(), extraJson);

    LOG_I(MODULE_PREFIX, "handleFileUploadStartMsg reqStr %s filename %s fileLen %d streamID %d", 
                _reqStr.c_str(),
                _fileName.c_str(), 
                _fileSize,
                _streamID);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File upload ended normally
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolFileStream::handleFileUploadEndMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame)
{
    // Handle file end
#ifdef DEBUG_RICREST_FILEUPLOAD
    uint32_t blocksSent = cmdFrame.getLong("blockCount", 0);
#endif

    // Callback to indicate end of activity
    if (_fileRxCancelEndCB)
        _fileRxCancelEndCB(true);

    // Response
    Utils::setJsonBoolResult(reqStr.c_str(), respMsg, true);

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

void ProtocolFileStream::handleFileUploadCancelMsg(const String& reqStr, String& respMsg, const JSONParams& cmdFrame)
{
    // Handle file cancel
    String fileName = cmdFrame.getString("fileName", "");
    String reason = cmdFrame.getString("reason", "");

    // Cancel upload
    uploadCancel(reason.c_str());

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

bool ProtocolFileStream::validateUploadStart(String& reqStr, String& fileName, uint32_t fileSize, 
        uint32_t channelID, String& respInfo, uint32_t crc16, bool crc16Valid)
{
    // Check if already in progress
    if (_isUploading && (_expectedFilePos > 0))
    {
        respInfo = "uploadInProgress";
        return false;
    }
    

    // File params
    _reqStr = reqStr;
    _fileName = fileName;
    _fileSize = fileSize;
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

void ProtocolFileStream::uploadService(bool& genAck)
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
                        _bytesCount, getOkTo(), _lastMsgMs, nowMillis, _blockCount, _blockSize, _batchAckSize, _batchBlockAckRetry);
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
// Validate received block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolFileStream::validateRxBlock(uint32_t filePos, uint32_t blockLen, bool& blockValid, 
            bool& isFirstBlock, bool& isFinalBlock, bool& genAck)
{
    // Check uploading
    if (!_isUploading)
        return;

    // Returned vals
    blockValid = false;
    isFinalBlock = false;
    isFirstBlock = false;
    genAck = false;

    // Add to batch
    _batchBlockCount++;
    _lastMsgMs = millis();

    // Check
    if (filePos != _expectedFilePos)
    {
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_W(MODULE_PREFIX, "validateRxBlock unexpected %d != %d, blockCount %d batchBlockCount %d, batchAckSize %d", 
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
        isFirstBlock = filePos == 0;

        // Check if this is the final block
        isFinalBlock = checkFinalBlock(filePos, blockLen);

#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
        LOG_I(MODULE_PREFIX, "validateRxBlock ok blockCount %d batchBlockCount %d, batchAckSize %d",
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

void ProtocolFileStream::uploadCancel(const char* reasonStr)
{
    // End upload state machine
    uploadEnd();

    // Callback to indicate cancellation
    if (_fileRxCancelEndCB)
        _fileRxCancelEndCB(false);

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
                    0, MSG_TYPE_RESPONSE);

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

void ProtocolFileStream::uploadEnd()
{
    _isUploading = false;
    _debugFinalMsgToSend = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get OkTo
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t ProtocolFileStream::getOkTo()
{
    return _expectedFilePos;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get block info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double ProtocolFileStream::getBlockRate()
{
    uint32_t elapsedMs = millis() - _startMs;
    if (elapsedMs > 0)
        return 1000.0*_blockCount/elapsedMs;
    return 0;
}
bool ProtocolFileStream::checkFinalBlock(uint32_t filePos, uint32_t blockLen)
{
    return filePos + blockLen >= _fileSize;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug and Stats
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolFileStream::debugStatsReady()
{
    return _debugFinalMsgToSend || (_isUploading && Utils::isTimeout(millis(), _debugLastStatsMs, DEBUG_STATS_MS));
}

String ProtocolFileStream::debugStatsStr()
{
    char outStr[200];
    snprintf(outStr, sizeof(outStr), 
            R"("actv":%d,"msgRate":%.1f,"dataBps":%.1f,"bytes":%d,"blks":%d,"blkSize":%d,"strmID":%d,"name":"%s")",
            _isUploading,
            statsFinalMsgRate(), 
            statsFinalDataRate(), 
            _bytesCount,
            _blockCount, 
            _blockSize,
            _streamID,
            _fileName.c_str());
    statsEndWindow();
    _debugLastStatsMs = millis();
    _debugFinalMsgToSend = false;
    return outStr;
}

double ProtocolFileStream::statsMsgRate()
{
    uint32_t winMs = millis() - _statsWindowStartMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _blocksInWindow / winMs;
}

double ProtocolFileStream::statsDataRate()
{
    uint32_t winMs = millis() - _statsWindowStartMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _bytesInWindow / winMs;
}

double ProtocolFileStream::statsFinalMsgRate()
{
    uint32_t winMs = _lastMsgMs - _startMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _blockCount / winMs;
}

double ProtocolFileStream::statsFinalDataRate()
{
    uint32_t winMs = _lastMsgMs - _startMs;
    if (winMs == 0)
        return 0;
    return 1000.0 * _bytesCount / winMs;
}

void ProtocolFileStream::statsEndWindow()
{
    _blocksInWindow = 0;
    _bytesInWindow = 0;
    _statsWindowStartMs = millis();
}

