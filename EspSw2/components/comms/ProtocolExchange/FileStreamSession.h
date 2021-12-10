/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// File Stream Session
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Utils.h>
#include <ArduinoTime.h>
#include "FileStreamBlock.h"
#include "FileSystemChunker.h"
#include "ProtocolFileStream.h"

class FileStreamSession
{
public:
    // Constructor / destructor
    FileStreamSession(const String& filename, uint32_t channelID, bool useProtocolFileStream,
                ProtocolEndpointManager* pProtocolEndpointManager,
                SysModBase* pFileHandler, SysModBase* pStreamHandler, SysModBase* pFirmwareUpdater,
                ProtocolFileStream::FileStreamType fileStreamType, uint32_t streamID);
    virtual ~FileStreamSession();

    // Info
    bool isActive()
    {
        return _isActive;
    }
    const String& getFileStreamName()
    {
        return _fileStreamName;
    }
    uint32_t getChannelID()
    {
        return _channelID;
    }
    uint32_t getStreamID()
    {
        return _protocolFileStream.getStreamID();
    }
    bool isMainFWUpdate()
    {
        return _fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_FIRMWARE;
    }
    bool isFileSystemActivity()
    {
        return _fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_FILE;
    }
    bool isStreaming()
    {
        return (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_RELIABLE_STREAM) || 
                    (_fileStreamType == ProtocolFileStream::FILE_STREAM_TYPE_RT_STREAM);
    }
    bool isUpload()
    {
        return true;
    }
    void service();

    // Handle command frame
    bool handleCmdFrame(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg)
    {
        bool rsltOk = _protocolFileStream.handleCmdFrame(cmdName, ricRESTReqMsg, respMsg, endpointMsg);
        if (!_protocolFileStream.isBusy())
            _isActive = false;
        return rsltOk;
    }

    // Handle file/stream block message
    bool handleFileStreamBlockMsg(RICRESTMsg& RICRESTReqMsg, String& respMsg)
    {
        return _protocolFileStream.handleFileStreamBlockMsg(RICRESTReqMsg, respMsg);
    }

    // Handle file upload block and cancel
    bool fileStreamBlockWrite(FileStreamBlock& fileStreamBlock);
    void fileStreamCancelEnd(bool isNormalEnd);

    // Debug
    String getDebugJSON();
    
private:
    // Is active
    bool _isActive = false;

    // Using protocol file/stream
    bool _useProtocolFileStream;

    // File/stream name and type
    String _fileStreamName;
    ProtocolFileStream::FileStreamType _fileStreamType;

    // Channel ID
    uint32_t _channelID;

    // Protocol handler
    ProtocolFileStream _protocolFileStream;

    // Chunker
    FileSystemChunker* _pFileChunker;

    // Handlers
    SysModBase* _pFileHandler;
    SysModBase* _pStreamHandler;
    SysModBase* _pFirmwareUpdater;

    // Session idle handler
    uint32_t _sessionLastActiveMs;
    static const uint32_t MAX_SESSION_IDLE_TIME_MS = 10000;

    // Stats
    uint32_t _startTimeMs;
    uint64_t _totalWriteTimeUs;
    uint32_t _totalBytes;
    uint32_t _totalChunks;

};
