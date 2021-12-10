/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolExchange
// Hub for handling protocol endpoint messages
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "ProtocolExchange.h"
#include "ProtocolEndpointMsg.h"
#include <ProtocolEndpointManager.h>
#include <RestAPIEndpointManager.h>
#include <ProtocolRICSerial.h>
#include <ProtocolRICFrame.h>
#include <ProtocolRICJSON.h>
#include <RICRESTMsg.h>
#include <SysManager.h>

static const char* MODULE_PREFIX = "ProtExchg";

// Warn
#define WARN_ON_SLOW_PROC_ENDPOINT_MESSAGE
#define WARN_ON_FILE_UPLOAD_FAILED

// Debug
// #define DEBUG_RICREST_MESSAGES
// #define DEBUG_RICREST_MESSAGES_RESPONSE
// #define DEBUG_ENDPOINT_MESSAGES
// #define DEBUG_SLOW_PROC_ENDPOINT_MESSAGE_DETAIL
// #define DEBUG_RICREST_CMDFRAME
// #define DEBUG_FILE_STREAM_SESSIONS

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolExchange::ProtocolExchange(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    // Handlers
    _pFileHandler = nullptr;
    _pStreamHandler = nullptr;
    _pFirmwareUpdater = nullptr;
    _nextStreamID = ProtocolFileStream::FILE_STREAM_ID_MIN;
    _sysManStateIndWasActive = false;
}

ProtocolExchange::~ProtocolExchange()
{
    // Remove sessions
    for (FileStreamSession* pSession : _sessions)
    {
        delete pSession;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolExchange::service()
{
    // Service active sessions

    bool isMainFWUpdate = false;
    bool isFileSystemActivity = false;
    bool isStreaming = false;
    for (FileStreamSession* pSession : _sessions)
    {
        // Service the session
        pSession->service();

        // Update status
        isMainFWUpdate = pSession->isMainFWUpdate();
        isFileSystemActivity = pSession->isFileSystemActivity();
        isStreaming = pSession->isStreaming();

        // Check if the session is inactive
        if (!pSession->isActive())
        {
#ifdef DEBUG_FILE_STREAM_SESSIONS
            LOG_I(MODULE_PREFIX, "service session inactive name %s channel %d streamID %d",
                        pSession->getFileStreamName().c_str(), pSession->getChannelID(), pSession->getStreamID());
#endif            
            // Tidy up inactive session
            _sessions.remove(pSession);
            delete pSession;

            // Must break here so list iteration isn't hampered
            break;
        }
    }

    // Inform SysManager of changes in file/stream and firmware update activity
    bool isActive = isMainFWUpdate || isFileSystemActivity || isStreaming;
    if (_sysManStateIndWasActive != isActive)
    {
        if (getSysManager())
            getSysManager()->informOfFileStreamActivity(isMainFWUpdate, isFileSystemActivity, isStreaming);
        _sysManStateIndWasActive = isActive;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get info JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ProtocolExchange::getDebugJSON()
{
    String jsonStr;
    for (FileStreamSession* pSession : _sessions)
    {
        if (jsonStr.length() != 0)
            jsonStr += ",";
        jsonStr += pSession->getDebugJSON();
    }
    return "[" + jsonStr + "]";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Endpoints
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolExchange::addProtocolEndpoints(ProtocolEndpointManager& endpointManager)
{
    // Add support for RICSerial
    String ricSerialConfig = configGetString("RICSerial", "{}");
    LOG_I(MODULE_PREFIX, "addProtocolEndpoints - adding RICSerial config %s", ricSerialConfig.c_str());
    ProtocolDef ricSerialProtocolDef = { ProtocolRICSerial::getProtocolNameStatic(), ProtocolRICSerial::createInstance, ricSerialConfig, 
                        std::bind(&ProtocolExchange::processEndpointMsg, this, std::placeholders::_1) };
    if (getProtocolEndpointManager())
        getProtocolEndpointManager()->addProtocol(ricSerialProtocolDef);

    // Add support for RICFrame
    String ricFrameConfig = configGetString("RICFrame", "{}");
    LOG_I(MODULE_PREFIX, "addProtocolEndpoints - adding RICFrame config %s", ricFrameConfig.c_str());
    ProtocolDef ricFrameProtocolDef = { ProtocolRICFrame::getProtocolNameStatic(), ProtocolRICFrame::createInstance, ricFrameConfig, 
                        std::bind(&ProtocolExchange::processEndpointMsg, this, std::placeholders::_1) };
    if (getProtocolEndpointManager())
        getProtocolEndpointManager()->addProtocol(ricFrameProtocolDef);

    // Add support for RICJSON
    String ricJSONConfig = configGetString("RICJSON", "{}");
    LOG_I(MODULE_PREFIX, "addProtocolEndpoints - adding RICJSON config %s", ricJSONConfig.c_str());
    ProtocolDef ricJSONProtocolDef = { ProtocolRICJSON::getProtocolNameStatic(), ProtocolRICJSON::createInstance, ricFrameConfig,
                        std::bind(&ProtocolExchange::processEndpointMsg, this, std::placeholders::_1) };
    if (getProtocolEndpointManager())
        getProtocolEndpointManager()->addProtocol(ricJSONProtocolDef);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file/stream data block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::handleAPIFileStreamBlock(const String& req, FileStreamBlock& fileStreamBlock, 
            const APISourceInfo& sourceInfo, ProtocolFileStream::FileStreamType fileStreamType)
{
    // See if this is the first block
    if (fileStreamBlock.firstBlock)
    {
        // Get a new session
        if (!getFileStreamNewSession(fileStreamBlock.filename, 
                        sourceInfo.channelID, fileStreamType, false))
            return false;
    }

    // Get the session
    FileStreamSession* pSession = getFileStreamExistingSession(fileStreamBlock.filename, sourceInfo.channelID, 
                    ProtocolFileStream::FILE_STREAM_ID_ANY);
    if (!pSession)
        return false;
    
    // Handle the block
    return pSession->fileStreamBlockWrite(fileStreamBlock);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process endpoint message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processEndpointMsg(ProtocolEndpointMsg &cmdMsg)
{
    // Handle the command message
    ProtocolMsgProtocol protocol = cmdMsg.getProtocol();

#ifdef WARN_ON_SLOW_PROC_ENDPOINT_MESSAGE
    uint32_t msgProcStartTimeMs = millis();
#endif

#ifdef DEBUG_ENDPOINT_MESSAGES
    // Debug
    LOG_I(MODULE_PREFIX, "Message received protocol %s msgNum %d msgType %d len %d", 
    		ProtocolEndpointMsg::getProtocolAsString(protocol), 
    		cmdMsg.getMsgNumber(), cmdMsg.getMsgTypeCode(),
    		cmdMsg.getBufLen());
#endif

    // Handle ROSSerial
    if (protocol == MSG_PROTOCOL_ROSSERIAL)
    {
        // Not implemented as ROSSERIAL is unused in this direction
    }
    else if (protocol == MSG_PROTOCOL_RICREST)
    {
        // Extract request msg
        RICRESTMsg ricRESTReqMsg;
        ricRESTReqMsg.decode(cmdMsg.getBuf(), cmdMsg.getBufLen());

#ifdef DEBUG_RICREST_MESSAGES
        LOG_I(MODULE_PREFIX, "RICREST rx elemCode %d", ricRESTReqMsg.getElemCode());
#endif

        // Check elemCode of message
        String respMsg;
        switch(ricRESTReqMsg.getElemCode())
        {
            case RICRESTMsg::RICREST_ELEM_CODE_URL:
            {
                processRICRESTURL(ricRESTReqMsg, respMsg, APISourceInfo(cmdMsg.getChannelID()));
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_BODY:
            {
                processRICRESTBody(ricRESTReqMsg, respMsg, APISourceInfo(cmdMsg.getChannelID()));
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON:
            {
                // This message type is reserved for responses
                LOG_W(MODULE_PREFIX, "processEndpointMsg rx RICREST JSON reserved for response");
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_COMMAND_FRAME:
            {
                processRICRESTCmdFrame(ricRESTReqMsg, respMsg, cmdMsg);
                break;
            }
            case RICRESTMsg::RICREST_ELEM_CODE_FILEBLOCK:
            {
                processRICRESTFileStreamBlock(ricRESTReqMsg, respMsg, cmdMsg);
                break;
            }
        }

        // Check for response
        if (respMsg.length() != 0)
        {
            // Send the response back
            ProtocolEndpointMsg endpointMsg;
            RICRESTMsg::encode(respMsg, endpointMsg, RICRESTMsg::RICREST_ELEM_CODE_CMDRESPJSON);
            endpointMsg.setAsResponse(cmdMsg);

            // Send message on the appropriate channel
            if (getProtocolEndpointManager())
                getProtocolEndpointManager()->handleOutboundMessage(endpointMsg);

            // Debug
#ifdef DEBUG_RICREST_MESSAGES_RESPONSE
            LOG_I(MODULE_PREFIX, "processEndpointMsg restAPI msgLen %d response %s", 
                        ricRESTReqMsg.getBinLen(), endpointMsg.getBuf());
#endif
        }
    }

#ifdef WARN_ON_SLOW_PROC_ENDPOINT_MESSAGE
    if (Utils::isTimeout(millis(), msgProcStartTimeMs, MSG_PROC_SLOW_PROC_THRESH_MS))
    {
#ifdef DEBUG_SLOW_PROC_ENDPOINT_MESSAGE_DETAIL
        String msgHex;
        Utils::getHexStrFromBytes(cmdMsg.getBuf(), cmdMsg.getBufLen(), msgHex);
#endif
        LOG_W(MODULE_PREFIX, "processEndpointMsg SLOW took %ldms protocol %d len %d msg %s", 
                Utils::timeElapsed(millis(), msgProcStartTimeMs),
                protocol,
                cmdMsg.getBufLen(),
#ifdef DEBUG_SLOW_PROC_ENDPOINT_MESSAGE_DETAIL
                msgHex.c_str()
#else
                ""
#endif
                );
    }
#endif
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg URL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTURL(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo)
{
    // Handle via standard REST API
    if (getRestAPIEndpoints())
        return getRestAPIEndpoints()->handleApiRequest(ricRESTReqMsg.getReq().c_str(), respMsg, sourceInfo);
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg URL
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTBody(RICRESTMsg& ricRESTReqMsg, String& respMsg, const APISourceInfo& sourceInfo)
{
    // NOTE - implements POST for RICREST - not currently needed

//     // Handle the body
//     String reqStr;
//     uint32_t bufferPos = ricRESTReqMsg.getBufferPos();
//     const uint8_t* pBuffer = ricRESTReqMsg.getBinBuf();
//     uint32_t bufferLen = ricRESTReqMsg.getBinLen();
//     uint32_t totalBytes = ricRESTReqMsg.getTotalBytes();
//     bool rsltOk = false;
//     if (pBuffer && _pRestAPIEndpointManager)
//     {
//         _pRestAPIEndpointManager->handleApiRequestBody(reqStr, pBuffer, bufferLen, bufferPos, totalBytes, sourceInfo);
//         rsltOk = true;
//     }

//     // Response
//     Utils::setJsonBoolResult(pReqStr, respMsg, rsltOk);

//     // Debug
// // #ifdef DEBUG_RICREST_MESSAGES
//     LOG_I(MODULE_PREFIX, "addCommand restBody binBufLen %d bufferPos %d totalBytes %d", 
//                 bufferLen, bufferPos, totalBytes);
// // #endif
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTCmdFrame(RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg)
{
    // ChannelID
    uint32_t channelID = endpointMsg.getChannelID();

    // Check file/stream messages
    String fileStreamName;
    ProtocolFileStream::FileStreamType fileStreamType = ProtocolFileStream::FILE_STREAM_TYPE_FILE;
    String cmdName;
    uint32_t streamID = ProtocolFileStream::FILE_STREAM_ID_ANY;
    ProtocolFileStream::FileStreamMsgType fileStreamMsgType = 
                    ProtocolFileStream::getFileStreamMsgType(ricRESTReqMsg, cmdName, fileStreamName, fileStreamType, streamID);

    // Handle non-file-stream messages
    if (fileStreamMsgType == ProtocolFileStream::FILE_STREAM_MSG_TYPE_NONE)
        return processRICRESTNonFileStream(cmdName, ricRESTReqMsg, respMsg, endpointMsg);

    // Handle file stream
    FileStreamSession* pSession = nullptr;
    switch (fileStreamMsgType)
    {
        case ProtocolFileStream::FILE_STREAM_MSG_TYPE_START:
            pSession = getFileStreamNewSession(fileStreamName.c_str(), channelID, fileStreamType, true);
            break;
        default:
            pSession = getFileStreamExistingSession(fileStreamName.c_str(), channelID, streamID);
            break;
    }

    // Check session is valid
    if (!pSession)
    {
        // Always return success for these messages as a reboot on new firmware may have occurred
        // and the session may already be closed
        Utils::setJsonBoolResult(ricRESTReqMsg.getReq().c_str(), respMsg, true);
        return true;
    }

    // Session is valid so send message to it
    return pSession->handleCmdFrame(cmdName, ricRESTReqMsg, respMsg, endpointMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg file/stream block
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTFileStreamBlock(RICRESTMsg& ricRESTReqMsg, String& respMsg, ProtocolEndpointMsg &cmdMsg)
{
    // Extract streamID
    uint32_t streamID = ricRESTReqMsg.getstreamID();

    // Find corresponding session
    FileStreamSession* pSession = findFileStreamSession(streamID, nullptr, cmdMsg.getChannelID());
    if (!pSession)
    {
        LOG_W(MODULE_PREFIX, "processRICRESTCmdFrame session not found for streamID %d", streamID);
        return false;
    }

    // Handle message
    return pSession->handleFileStreamBlockMsg(ricRESTReqMsg, respMsg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame that are non-file-stream messages
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolExchange::processRICRESTNonFileStream(const String& cmdName, RICRESTMsg& ricRESTReqMsg, String& respMsg, 
                const ProtocolEndpointMsg &endpointMsg)
{
    // Convert to REST API query string - it won't necessarily be a valid query string for external use
    // but works fine for internal use
    String reqStr = cmdName;
    String queryStr = RdJson::getHTMLQueryFromJSON(ricRESTReqMsg.getPayloadJson());
    if (queryStr.length() > 0)
        reqStr += "?" + queryStr;

    // Handle via standard REST API
    if (getRestAPIEndpoints())
        return getRestAPIEndpoints()->handleApiRequest(reqStr.c_str(), respMsg, APISourceInfo(endpointMsg.getChannelID()));
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process RICRESTMsg CmdFrame that are non-file-stream messages
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession* ProtocolExchange::findFileStreamSession(uint32_t streamID, const char* fileStreamName, uint32_t channelID)
{
    // First check the case where we know the streamID
    if (streamID != ProtocolFileStream::FILE_STREAM_ID_ANY)
    {
        for (FileStreamSession* pSession : _sessions)
        {
            if (pSession->getStreamID() == streamID)
                return pSession;
        }
        return nullptr;
    }

    // Check for matching filename and channel
    for (FileStreamSession* pSession : _sessions)
    {
        if (pSession && 
                ((!fileStreamName || (strlen(fileStreamName) == 0) || pSession->getFileStreamName().equals(fileStreamName))) &&
                (pSession->getChannelID() == channelID))
        {
            return pSession;
        }
    }
    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file/stream start condition
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession* ProtocolExchange::getFileStreamNewSession(const char* fileStreamName, uint32_t channelID, 
                ProtocolFileStream::FileStreamType fileStreamType, bool useProtocolFileStream)
{
    // Check existing sessions
    if (findFileStreamSession(ProtocolFileStream::FILE_STREAM_ID_ANY, fileStreamName, channelID))
    {
        // If we find one then ignore this as it is a re-start of an existing session
        LOG_W(MODULE_PREFIX, "processRICRESTCmdFrame re-start existing - ignored name %s channelID %d",
                        fileStreamName, channelID);
        return nullptr;
    }

    // Check number of sessions
    if (_sessions.size() > MAX_SIMULTANEOUS_FILE_STREAM_SESSIONS)
    {
        // Max sessions already active
        LOG_W(MODULE_PREFIX, "processRICRESTCmdFrame max active - ignored name %s channelID %d",
                        fileStreamName, channelID);
        return nullptr;
    }

    // Create new session
    FileStreamSession* pSession = new FileStreamSession(fileStreamName, channelID, useProtocolFileStream,
                getProtocolEndpointManager(),
                _pFileHandler, _pStreamHandler, _pFirmwareUpdater, 
                fileStreamType, _nextStreamID);
    if (!pSession)
    {
        LOG_W(MODULE_PREFIX, "processRICRESTCmdFrame failed to create session name %s channelID %d",
                        fileStreamName, channelID);
        return nullptr;
    }

    // Debug
#ifdef DEBUG_FILE_STREAM_SESSIONS
    LOG_I(MODULE_PREFIX, "processRICRESTCmdFrame START name %s channelID %d streamID %d streamType %d", 
                        fileStreamName, channelID, _nextStreamID, fileStreamType);
#endif

    // Add to session list
    _sessions.push_back(pSession);

    // Bump the stream ID
    _nextStreamID++;
    if (_nextStreamID >= ProtocolFileStream::FILE_STREAM_ID_MAX)
        _nextStreamID = ProtocolFileStream::FILE_STREAM_ID_MIN;

    // Session is valid so send message to it
    return pSession;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle file/stream end
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FileStreamSession* ProtocolExchange::getFileStreamExistingSession(const char* fileStreamName, uint32_t channelID, uint32_t streamID)
{
    // Debug
#ifdef DEBUG_FILE_STREAM_SESSIONS
    LOG_I(MODULE_PREFIX, "processRICRESTCmdFrame EXISTING name %s channelID %d streamID %d", 
                fileStreamName, channelID, streamID);
#endif

    // Find existing session
    FileStreamSession* pSession = findFileStreamSession(streamID, fileStreamName, channelID);
    if (!pSession)
    {
        LOG_W(MODULE_PREFIX, "processRICRESTCmdFrame session not found for fileStreamName %s channelID %d",
                                    fileStreamName, channelID);
        return nullptr;
    }
    return pSession;
}
