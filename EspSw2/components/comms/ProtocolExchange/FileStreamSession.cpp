/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Stream Session
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FileStreamSession.h"
#include "SysModBase.h"

static const char* MODULE_PREFIX = "FSSess";

class ProtocolEndpointManager;

// Warn
#define WARN_ON_FW_UPDATE_FAILED
#define WARN_ON_STREAM_FAILED

// Debug
// #define DEBUG_FILE_STREAM_BLOCK

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession::FileStreamSession(const String& filename, uint32_t channelID, bool useProtocolFileStream,
                ProtocolEndpointManager* pProtocolEndpointManager,
                SysModBase* pFileHandler, SysModBase* pStreamHandler, SysModBase* pFirmwareUpdater,
                ProtocolFileStream::FileStreamType fileStreamType, uint32_t streamID) :
        _protocolFileStream(
            std::bind(&FileStreamSession::fileStreamBlockWrite, this, std::placeholders::_1),
            std::bind(&FileStreamSession::fileStreamCancelEnd, this, std::placeholders::_1),
            pProtocolEndpointManager,
            fileStreamType, streamID)
{
    _isActive = true;
    _useProtocolFileStream = useProtocolFileStream;
    _sessionLastActiveMs = millis();
    _fileStreamName = filename;
    _channelID = channelID;
    _fileStreamType = fileStreamType;
    _startTimeMs = millis();
    _totalWriteTimeUs = 0;
    _totalBytes = 0;
    _totalChunks = 0;
    _pFileChunker = nullptr;
    _pFileHandler = pFileHandler;
    _pStreamHandler = pStreamHandler;
    _pFirmwareUpdater = pFirmwareUpdater;

    // For file handling use a FileSystemChunker to access the file
    if (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_FILE)
    {
        _pFileChunker = new FileSystemChunker();
        if (_pFileChunker)
        {
            _pFileChunker->start(filename, 0, false, true, true);
        }
    }
}

FileStreamSession::~FileStreamSession()
{
    // Tidy up
    delete _pFileChunker;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle command frame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileStreamSession::service()
{
    // Service file/stream protocol
    _protocolFileStream.service();

    // Check for timeouts
    if (_isActive && Utils::isTimeout(millis(), _sessionLastActiveMs, MAX_SESSION_IDLE_TIME_MS))
    {
        _isActive = false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// getDebugJSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String FileStreamSession::getDebugJSON()
{
    // Handle uploads using file/stream protocol
    if (_useProtocolFileStream)
        return _protocolFileStream.getDebugJSON(true);

    // Other uploads
    uint32_t elapMs = _sessionLastActiveMs - _startTimeMs;
    char outStr[200];
    snprintf(outStr, sizeof(outStr), 
            R"({"actv":%d,"dataBps":%.1f,"writeBps":%.1f,"chunks":%d,"bytes":%d,"strmID":%d})",
            _isActive,
            elapMs > 0 ? _totalBytes * 1000.0 / elapMs : 0,
            _totalWriteTimeUs > 0 ? _totalBytes * 1000000.0 / _totalWriteTimeUs : 0,
            _totalChunks,
            _totalBytes,
            _protocolFileStream.getStreamID());
    return outStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File/stream block write - callback from ProtocolFileStream
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileStreamSession::fileStreamBlockWrite(FileStreamBlock& fileStreamBlock)
{
#ifdef DEBUG_FILE_STREAM_BLOCK
    LOG_I(MODULE_PREFIX, "fileStreamBlockWrite pos %d len %d (%s) fileStreamType %d isFirst %d isFinal %d name %s",
                fileStreamBlock.filePos, fileStreamBlock.fileLen, fileStreamBlock.fileLenValid ? "Y" : "N",
                _fileStreamType, fileStreamBlock.firstBlock, fileStreamBlock.finalBlock, fileStreamBlock.filename);
#endif

    // Check for first block
    if (fileStreamBlock.firstBlock)
        _startTimeMs = millis();

    // Check for final block
    if (fileStreamBlock.finalBlock)
        _isActive = false;

    // Keep session alive while we're receiving
    _sessionLastActiveMs = millis();

    // Update stats
    _totalChunks++;

    // Check if firmware and handle if so
    if (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_FIRMWARE)
    {
        // Firmware updater valid?
        if (!_pFirmwareUpdater)
            return false;

        // Check if this is the first block
        if (fileStreamBlock.firstBlock)
        {
            // Start OTA update
            // For ESP32 there will be a long delay at this point - around 4 seconds so
            // the block will not get acknowledged until after that
            if (!_pFirmwareUpdater->fileStreamStart(fileStreamBlock.filename, fileStreamBlock.fileLen))
            {
#ifdef WARN_ON_FW_UPDATE_FAILED
                LOG_W(MODULE_PREFIX, "fileStreamBlockWrite FW start FAILED name %s len %d",
                                fileStreamBlock.filename, fileStreamBlock.fileLen);
#endif
                return false;
            }
        }
        uint64_t startUs = micros();
        bool fwRslt = _pFirmwareUpdater->fileStreamDataBlock(fileStreamBlock);
        _totalBytes += fileStreamBlock.blockLen;
        _totalWriteTimeUs += micros() - startUs;
        return fwRslt;
    }

    // Check for File and handle if so
    if (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_FILE)
    {
        // Write using the chunker
        if (!_pFileChunker)
            return false;

        uint32_t bytesWritten = 0; 
        uint64_t startUs = micros();
        bool chunkerRslt = _pFileChunker->nextWrite(fileStreamBlock.pBlock, fileStreamBlock.blockLen, 
                        bytesWritten, fileStreamBlock.finalBlock);
        _totalBytes += bytesWritten;
        _totalWriteTimeUs += micros() - startUs;
        return chunkerRslt;
    }

    // Must be a stream
    if (!_pStreamHandler)
        return false;

    // Check if this is the first block
    if (fileStreamBlock.firstBlock)
    {
        if (!_pStreamHandler->fileStreamStart(fileStreamBlock.filename, fileStreamBlock.fileLen))
        {
#ifdef WARN_ON_STREAM_FAILED
            LOG_W(MODULE_PREFIX, "fileStreamBlockWrite STREAM start FAILED name %s len %d",
                            fileStreamBlock.filename, fileStreamBlock.fileLen);
#endif
            return false;
        }
    }

    // Handle block
    uint32_t bytesWritten = 0;
    uint64_t startUs = micros();
    bool streamRslt = _pStreamHandler->fileStreamDataBlock(fileStreamBlock);
    _totalWriteTimeUs += micros() - startUs;
    _totalBytes += bytesWritten;

    // Check if this is the final block
    if (fileStreamBlock.finalBlock)
        _pStreamHandler->fileStreamCancelEnd(true);

    return streamRslt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// file/stream cancel - callback from ProtocolFileStream
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FileStreamSession::fileStreamCancelEnd(bool isNormalEnd)
{
    // Session no longer active
    _isActive = false;

    // File end is handled by ProtocolFileStream

    // Check if we should cancel a firmware update
    if (_pFirmwareUpdater && (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_FIRMWARE))
    {
        _pFirmwareUpdater->fileStreamCancelEnd(isNormalEnd);
        return;
    }

    // Check if we should cancel a stream
    if (_pStreamHandler && ((_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_RT_STREAM) ||
            (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_RELIABLE_STREAM)))
    {
        _pStreamHandler->fileStreamCancelEnd(isNormalEnd);
        return;
    }

}