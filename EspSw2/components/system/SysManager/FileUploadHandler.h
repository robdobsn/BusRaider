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

// #define DEBUG_RICREST_FILEUPLOAD 1
// #define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK 1
// #define DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK_DETAIL 1

#define FILE_UPLOAD_USE_BATCH_ACKS

class FileUploadHandler
{
public:
    static const uint32_t UPLOAD_FAIL_TIMEOUT = 60000;
    static const uint32_t FIRST_MSG_TIMEOUT = 3000;
    static const uint32_t BLOCK_MSGS_TIMEOUT = 1000;
    static const uint32_t MAX_BATCH_BLOCK_ACK_RETRIES = 5;
    static const uint32_t FILE_BLOCK_SIZE = 220;
    static const uint32_t NUM_BLOCKS_TO_CACHE = 10;
    static const uint32_t BATCH_ACK_SIZE = 40;
    FileUploadHandler()
    {
        // File params
        _fileSize = 0;
        _fileIsRICFirmware = false;
        _commsChannelID = 0;

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

        // Debug
        _debugLastStatsMs = millis();
        _debugFinalMsgToSend = false;

        // Cache
        _cacheStartFilePos = 0;

        // Batch handling
        _expectedFilePos = 0;
        _batchAckSize = BATCH_ACK_SIZE;
        _batchBlockCount = 0;
        _batchBlockAckRetry = 0;
    }
    bool start(String& reqStr, String& fileName, uint32_t fileSize, 
            String& fileType, uint32_t& fileBlockSize, uint32_t& batchAckSize,
            uint32_t channelID, String& respInfo)
    {
#ifdef FILE_UPLOAD_USE_BATCH_ACKS
        if (_isUploading && (_expectedFilePos > 0))
        {
            respInfo = "uploadInProgress";
            return false;
        }
        fileBlockSize = FILE_BLOCK_SIZE;
        batchAckSize = BATCH_ACK_SIZE;
#else
        // Block handling
        fileBlockSize = _blockSize;
#endif

        // File params
        _reqStr = reqStr;
        _fileName = fileName;
        _fileSize = fileSize;
        _fileType = fileType;
        _fileIsRICFirmware = (_fileType.equalsIgnoreCase("ricfw"));
        _commsChannelID = channelID;

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

        // Debug
        _debugLastStatsMs = millis();
        _debugFinalMsgToSend = false;

        // Blocks
        _blockSize = FILE_BLOCK_SIZE;

        // Cache
        _cacheStartFilePos = 0;
        cacheInit(NUM_BLOCKS_TO_CACHE);

        // Batch handling
        _expectedFilePos = 0;
        _batchAckSize = BATCH_ACK_SIZE;
        _batchBlockCount = 0;
        _batchBlockAckRetry = 0;
        return true;
    }

    void blockRx(uint32_t filePos, uint32_t blockLen, bool& blockValid, bool& isFinalBlock, bool& genAck)
    {
        // Check uploading
        if (!_isUploading)
            return;

        // Returned vals
        blockValid = false;
        isFinalBlock = false;
        genAck = false;

#ifdef FILE_UPLOAD_USE_BATCH_ACKS
        // Add to batch
        _batchBlockCount++;
        _lastMsgMs = millis();

        // Check
        if (filePos != _expectedFilePos)
        {
#ifdef DEBUG_RICREST_FILEUPLOAD_BLOCK_ACK
            LOG_W("FileUploadHandler", "blockRx unexpected %d != %d, blockCount %d batchBlockCount %d, batchAckSize %d", 
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
            LOG_I("FileUploadHandler", "blockRx ok blockCount %d batchBlockCount %d, batchAckSize %d",
                        _blockCount, _batchBlockCount, _batchAckSize);
#endif
        }

        // Generate an ack on the first block received and on completion of each batch
        bool batchComplete = (_batchBlockCount == _batchAckSize) || (_blockCount == 1);
        if (batchComplete)
            _batchBlockCount = 0;
        _batchBlockAckRetry = 0;
        genAck = batchComplete;

#else
        _blockCount++;
        _bytesCount += blockLen;
        _blocksInWindow++;
        _bytesInWindow += blockLen;
        _lastMsgMs = millis();

        // Check if the block can be cached
        if (!((_cacheLowestPos >= filePos) && (_cacheLowestPos + _cacheMaxBlocks * _cacheBlockSize))
            return;

        // Check if already in the cache
        for (CacheBlock& blk : _cacheBlocks)
        {
            if (_cacheBlocks.addrInBlock(filePos))
                return;
        }
        // // Check cache
        // int32_t cacheIdx = (filePos - _cacheStartFilePos) / _blockSize;
        // if (cacheIdx < 0)
        // {
        //     LOG_W("FileUploadHandler", "CacheIdx < 0 rxFilePos %d cacheStart %d blockSize %d",
        //                 filePos, _cacheStartFilePos, _blockSize);
        // } else if (cacheIdx >= _cacheBlockOk.size())
        // {
        //     LOG_W("FileUploadHandler", "CacheIdx > Cache size rxFilePos %d cacheStart %d blockSize %d",
        //                 filePos, _cacheStartFilePos, _blockSize);
        // }
        // else if (cacheIdx == 0)
        // {
        //     LOG_W("FileUploadHandler", "CacheIdx OK moving on filePos",
        //                 filePos, _cacheStartFilePos, _blockSize);
        // }
#endif
    }

    void cancel() 
    {
        _isUploading = false;
        cacheClear();
    }
    void end()
    {
        _isUploading = false;
        _debugFinalMsgToSend = true; 
        cacheClear();
    }
    uint32_t getOkTo()
    {
        return _expectedFilePos;
    }

    void service(bool& genAck)
    {
        genAck = false;
        // Check valid
        if (!_isUploading)
            return;
        
#ifdef FILE_UPLOAD_USE_BATCH_ACKS
        // At the start of ESP firmware update there is a long delay (3s or so)
        // This occurs after reception of the first block
        // So need to ensure that there is a long enough timeout
        if (Utils::isTimeout(millis(), _lastMsgMs, _blockCount < 2 ? FIRST_MSG_TIMEOUT : BLOCK_MSGS_TIMEOUT))
        {
            _batchBlockAckRetry++;
            if (_batchBlockAckRetry < MAX_BATCH_BLOCK_ACK_RETRIES)
            {
                LOG_W("FileUploadHandler", "service blockMsgs timeOut - okto ack needed lastMsgMs %d curMs %ld",
                            _lastMsgMs, millis());
                _lastMsgMs = millis();
                genAck = true;
                return;
            }
            else
            {
                LOG_W("FileUploadHandler", "service blockMsgs ack failed after retries");
                _isUploading = false;
            }
        }
        if (Utils::isTimeout(millis(), _lastMsgMs, UPLOAD_FAIL_TIMEOUT))
        {
            LOG_W("FileUploadHandler", "service timeOut");
            _isUploading = false;
        }
#else
        // Check for timeout
        if (Utils::isTimeout(millis(), _lastMsgMs, UPLOAD_FAIL_TIMEOUT))
        {
            cacheClear();
            _isUploading = false;   
        }
#endif        
    }

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
            snprintf(outStr, sizeof(outStr), "Complete OverallMsgRate %.1f/s OverallDataRate %.1f Bytes/s, TotalMsgs %d, TotalBytes %d",
                    statsFinalMsgRate(), statsFinalDataRate(), _blockCount, _bytesCount);
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

    void cacheInit(uint32_t numBlocks)
    {
        _cacheVec.resize(numBlocks * _blockSize);
        _cacheBlockOk.resize(numBlocks);
        for (uint32_t i = 0; i < numBlocks; i++)
            _cacheBlockOk[i] = false;
        _cacheStartFilePos = 0;
    }

    void cacheClear()
    {
        _cacheVec.clear();
        _cacheBlockOk.clear();
    }

    // File info
    String _reqStr;
    uint32_t _fileSize;
    String _fileName;
    String _fileType;
    bool _fileIsRICFirmware;

    // Upload state
    bool _isUploading;
    uint32_t _startMs;
    uint32_t _lastMsgMs;
    uint32_t _batchAckSize;
    uint32_t _blockSize;
    uint32_t _commsChannelID;

    // Stats
    uint32_t _blockCount;
    uint32_t _bytesCount;
    uint32_t _blocksInWindow;
    uint32_t _bytesInWindow;
    uint32_t _statsWindowStartMs;

    // Batch handling
    uint32_t _expectedFilePos;
    uint32_t _batchBlockCount;
    uint32_t _batchBlockAckRetry;

    // Cache
    uint32_t _cacheStartFilePos;
    std::vector<bool> _cacheBlockOk;
    std::vector<uint8_t> _cacheVec;

    // Debug
    uint32_t _debugLastStatsMs;
    static const uint32_t DEBUG_STATS_MS = 1000;
    bool _debugFinalMsgToSend;
};