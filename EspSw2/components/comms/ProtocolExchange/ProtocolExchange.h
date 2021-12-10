/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolExchange
// Hub for handling protocol endpoint messages
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <WString.h>
#include <vector>
#include <ProtocolBase.h>
#include "ProtocolDef.h"
#include "ProtocolEndpointDef.h"
#include "SysModBase.h"
#include "ProtocolFileStream.h"
#include "FileStreamSession.h"

class APISourceInfo;

class ProtocolExchange : public SysModBase
{
public:
    ProtocolExchange(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~ProtocolExchange();

    // Set handlers
    void setHandlers(SysModBase* pFileHandler, SysModBase* pStreamHandler, SysModBase* pFirmwareUpdater)
    {
        _pFileHandler = pFileHandler;
        _pStreamHandler = pStreamHandler;
        _pFirmwareUpdater = pFirmwareUpdater;
    }

    // File/Stream Block
    bool handleAPIFileStreamBlock(const String& req, FileStreamBlock& fileStreamBlock, 
            const APISourceInfo& sourceInfo, ProtocolFileStream::FileStreamType fileStreamType);

protected:
    // Service - called frequently
    virtual void service() override final;

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager& endpointManager) override final;

    // Get debug info
    virtual String getDebugJSON();

private:
    // Handlers
    SysModBase* _pFileHandler;
    SysModBase* _pStreamHandler;
    SysModBase* _pFirmwareUpdater;

    // Next streamID to allocate to a stream session
    uint32_t _nextStreamID; 

    // Transfer sessions
    static const uint32_t MAX_SIMULTANEOUS_FILE_STREAM_SESSIONS = 3;
    std::list<FileStreamSession*> _sessions;

    // Previous activity indicator to keep SysManager up-to-date
    bool _sysManStateIndWasActive;

    // Threshold for determining if message processing is slow
    static const uint32_t MSG_PROC_SLOW_PROC_THRESH_MS = 50;

    // Process endpoint message
    bool processEndpointMsg(ProtocolEndpointMsg& msg);
    bool processRICRESTURL(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo);
    bool processRICRESTBody(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo);
    bool processRICRESTCmdFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                        const ProtocolEndpointMsg &endpointMsg);
    bool processRICRESTFileStreamBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg, ProtocolEndpointMsg &cmdMsg);
    bool processRICRESTNonFileStream(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg);

    // File/stream session handling
    FileStreamSession* findFileStreamSession(uint32_t streamID, const char* fileStreamName, uint32_t channelID);
    FileStreamSession* getFileStreamNewSession(const char* fileStreamName, uint32_t channelID, 
                    ProtocolFileStream::FileStreamType fileStreamType, bool useProtocolFileStream);
    FileStreamSession* getFileStreamExistingSession(const char* fileStreamName, uint32_t channelID, uint32_t streamID);
};
