// Bus Raider
// Rob Dobson 2018-2019

#include "CommandHandler.h"
#include "../System/nmalloc.h"
#include "../System/ee_sprintf.h"
#include "../System/logging.h"
#include "minihdlc.h"
#include "../System/rdutils.h"
#include "../System/RdStdCpp.h"
#include "../System/lowlib.h"
#include <string.h>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromCmdHandler[] = "CommandHandler";

// Comms sockets
CommsSocketInfo CommandHandler::_commsSockets[MAX_COMMS_SOCKETS];
int CommandHandler::_commsSocketCount = 0;

// Callbacks
CmdHandlerPutToSerialFnType* CommandHandler::_pPutToHDLCSerialFunction = NULL;

// Singleton command handler
CommandHandler* CommandHandler::_pSingletonCommandHandler = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
CommandHandler::CommandHandler() :
    _miniHDLC(hdlcPutChStatic, hdlcFrameRxStatic)
{   
    // Singleton
    _pSingletonCommandHandler = this;

    // File reception
    _receivedFileName[0] = 0;
    _pReceivedFileType[0] = 0;
    _receivedFileStartInfo[0] = 0;
    _pReceivedFileDataPtr = 0;
    _receivedFileBufSize = 0;
    _receivedFileBytesRx = 0;
    _receivedBlockCount = 0;
}

CommandHandler::~CommandHandler()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms Sockets
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Add a comms socket
int CommandHandler::commsSocketAdd(CommsSocketInfo& commsSocketInfo)
{
    // Check if all used
    if (_commsSocketCount >= MAX_COMMS_SOCKETS)
        return -1;

    // Add in available space
    _commsSockets[_commsSocketCount] = commsSocketInfo;
    int tmpCount = _commsSocketCount++;

    return tmpCount;
}

void CommandHandler::commsSocketEnable(int busSocket, bool enable)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _commsSocketCount))
        return;
    _commsSockets[busSocket].enabled = enable;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDLC interface functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::handleHDLCReceivedChars(const uint8_t* pBytes, int numBytes)
{
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->_miniHDLC.handleBuffer(pBytes, numBytes);
}

void CommandHandler::hdlcPutCh(uint8_t ch)
{
    if (_pPutToHDLCSerialFunction)
    {
        uint8_t buf[1];
        buf[0] = ch;
        _pPutToHDLCSerialFunction(buf, 1);
    }
}

void CommandHandler::hdlcFrameRxStatic(const uint8_t *frameBuffer, int frameLength)
{
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->hdlcFrameRx(frameBuffer, frameLength);
}

void CommandHandler::hdlcPutChStatic(uint8_t ch)
{
    // LogWrite(FromCmdHandler, LOG_VERBOSE, "hdlcPutChStatic");

    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->hdlcPutCh(ch);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDLC frame handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle frame received from HDLC
// This is a ascii character string (null terminated) followed by a byte buffer containing parameters
void CommandHandler::hdlcFrameRx(const uint8_t *pFrame, int frameLength)
{
    // LogWrite(FromCmdHandler, LOG_VERBOSE, "Rx %d bytes", frameLength);

    // Handle the frame - extract command string
    char commandString[CMD_HANDLER_MAX_CMD_STR_LEN+1];
    strlcpy(commandString, (const char*)pFrame, CMD_HANDLER_MAX_CMD_STR_LEN);
    int commandStringLen = strlen(commandString);
    const uint8_t* pParams = pFrame + commandStringLen + 1;
    int paramsLen = frameLength - commandStringLen - 1;
    if (paramsLen < 0)
        paramsLen = 0;

    // Process the command with parameters
    processCommand(commandString, pParams, paramsLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process command received over HDLC
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Process split-up command string and parameters
void CommandHandler::processCommand(const char* pCmdJson, const uint8_t* pParams, int paramsLen)
{
    // Get the command string from JSON
    static const int MAX_CMD_NAME_STR = 200;
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return;

    // Debug
    // LogWrite(FromCmdHandler, LOG_DEBUG, "processCommand JSON %s cmdName %s paramsLen %d", pCmdJson, cmdName, paramsLen); 

    // Handle commands
    if (strcasecmp(cmdName, "ufStart") == 0)
    {
        LogWrite(FromCmdHandler, LOG_VERBOSE, "processCommand fileStart"); 
        handleFileStart(pCmdJson);
        return;
    }
    else if (strcasecmp(cmdName, "ufBlock") == 0)
    {
        handleFileBlock(pCmdJson, pParams, paramsLen);
        return;
    }
    else if (strcasecmp(cmdName, "ufEnd") == 0)
    {
        handleFileEnd(pCmdJson);
        return;
    }

    // Remote protocol 
    char pCommandString[CMD_HANDLER_MAX_CMD_STR_LEN+1];
    pCommandString[0] = 0;
    bool rdpMessage = false;
    uint32_t rdpIndex = 0;
    if (strcasecmp(cmdName, "rdp") == 0)
    {
        // LogWrite(FromCmdHandler, LOG_DEBUG, "RDP message rx cmd %s cmdLen %d paramsStr %s paramslen %d", 
        //             pCmdJson, strlen(pCmdJson), pParams, paramsLen);
        static const int MAX_RDP_IDX_STR_LEN = 20;
        char rdpIdxStr[MAX_RDP_IDX_STR_LEN];
        if (jsonGetValueForKey("index", pCmdJson, rdpIdxStr, MAX_RDP_IDX_STR_LEN))
        {
            rdpIndex = strtol(rdpIdxStr, NULL, 10);
        }

        // Handle the rdp frame - extract command string
        strlcpy(pCommandString, (const char*)pParams, CMD_HANDLER_MAX_CMD_STR_LEN);
        int commandStringLen = strlen(pCommandString);
        paramsLen = paramsLen - commandStringLen - 1;
        pParams = pParams + commandStringLen;
        if (paramsLen < 0)
            paramsLen = 0;
        if (paramsLen > 0)
            pParams++;
        pCmdJson = pCommandString;
        rdpMessage = true;
    }
              
    // Offer to comms sockets
    static const int MAX_RESP_MSG_LEN = 2000;
    char respJson[MAX_RESP_MSG_LEN];
    respJson[0] = 0;
    commsSocketHandleRxMsg(pCmdJson, pParams, paramsLen, respJson, MAX_RESP_MSG_LEN);

        // LogWrite(FromCmdHandler, LOG_DEBUG, "CMDMSG rx cmd %s cmdLen %d paramsStr %s paramslen %d respJson %s", 
        //         pCmdJson, strlen(pCmdJson), pParams, paramsLen, respJson);

    // Handle response
    if (strlen(respJson) > 0)
    {
        if (rdpMessage)
        {
            // Send response back
            sendData("rdp", (const uint8_t*)respJson, strlen(respJson), rdpIndex);
        }
        else
        {
            static const int MAX_CMD_NAME_RESP_LEN = 100;
            char cmdNameResp[MAX_CMD_NAME_RESP_LEN];
            strlcpy(cmdNameResp, cmdName, MAX_CMD_NAME_RESP_LEN);
            strlcat(cmdNameResp, "Resp", MAX_CMD_NAME_RESP_LEN);
            sendWithJSON(cmdNameResp, respJson);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms socket handlers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::commsSocketHandleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen)
{
    if (_commsSocketCount == 0)
        LogWrite(FromCmdHandler, LOG_DEBUG, "RxMsg no sockets");
    bool messageHandled = false;
    // Iterate the comms sockets
    for (int i = 0; i < _commsSocketCount; i++)
    {
        if (!_commsSockets[i].enabled)
        {
            LogWrite(FromCmdHandler, LOG_DEBUG, "RxMsg sock disabled %d", i);
            continue;
        }
        if (_commsSockets[i].handleRxMsg)
        {
            messageHandled = _commsSockets[i].handleRxMsg(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);
        }
        else
        {
            if (i != 0)
               LogWrite(FromCmdHandler, LOG_DEBUG, "RxMsg sock null %d", i);
        }
        
        if (messageHandled)
            break;
    }
}

void CommandHandler::commsSocketHandleReceivedFile(const char* fileStartInfo, uint8_t* rxData, int rxBytes, bool isFirmware)
{
    bool messageHandled = false;
    // Iterate the comms sockets
    for (int i = 0; i < _commsSocketCount; i++)
    {
        if (!_commsSockets[i].enabled)
            continue;
        if (isFirmware)
        {
            if(_commsSockets[i].otaUpdateFn)
                messageHandled = _commsSockets[i].otaUpdateFn(rxData, rxBytes);                
        }
        else
        {
            // LogWrite(FromCmdHandler, LOG_DEBUG, "CommsSocket %d rxFile %d", i, _commsSockets[i].receivedFileFn);
            if (_commsSockets[i].receivedFileFn)
                messageHandled = _commsSockets[i].receivedFileFn(fileStartInfo, rxData, rxBytes);
        }
        if (messageHandled)
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File Reception - start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle reception of file start block
void CommandHandler::handleFileStart(const char* pCmdJson)
{
    // Start a file upload - get file name
    if (!jsonGetValueForKey("fileName", pCmdJson, _receivedFileName, MAX_FILE_NAME_STR))
        return;

    LogWrite(FromCmdHandler, LOG_VERBOSE, "ufStart File %s, toPtr %08x", 
                _receivedFileName, _pReceivedFileDataPtr);

    // Get file type
    if (!jsonGetValueForKey("fileType", pCmdJson, _pReceivedFileType, MAX_FILE_TYPE_STR))
        return;

    LogWrite(FromCmdHandler, LOG_VERBOSE, "ufStart FileType %s", 
                _receivedFileName);

    // Get file length
    char fileLenStr[MAX_INT_ARG_STR_LEN+1];
    if (!jsonGetValueForKey("fileLen", pCmdJson, fileLenStr, MAX_INT_ARG_STR_LEN))
        return;

    // LogWrite(FromCmdHandler, LOG_VERBOSE, "ufStart FileLenStr %s", 
    //             fileLenStr);

    int fileLen = strtol(fileLenStr, NULL, 10);
    if (fileLen <= 0)
        return;
    // LogWrite(FromCmdHandler, LOG_VERBOSE, "ufStart FileLen %d", 
    //             fileLen);

    // Copy start info
    strlcpy(_receivedFileStartInfo, pCmdJson, CMD_HANDLER_MAX_CMD_STR_LEN);

    // Allocate space for file
    if (_pReceivedFileDataPtr != NULL)
        nmalloc_free((void**)(&_pReceivedFileDataPtr));
    _pReceivedFileDataPtr = (uint8_t*)nmalloc_malloc(fileLen);

    _receivedFileBytesRx = 0;
    _receivedBlockCount = 0;
    if (_pReceivedFileDataPtr)
    {
        _receivedFileBufSize = fileLen;

        // Debug
        LogWrite(FromCmdHandler, LOG_VERBOSE, "efStart File %s, toPtr %08x, bufSize %d", 
                    _receivedFileName, _pReceivedFileDataPtr, _receivedFileBufSize);
    }
    else
    {
        _receivedFileBufSize = 0;
        LogWrite(FromCmdHandler, LOG_WARNING, "efStart unable to allocate memory for file %s, bufSize %d", 
                    _receivedFileName, _receivedFileBufSize);

    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File Reception - chunk
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::handleFileBlock(const char* pCmdJson, const uint8_t* pData, int dataLen)
{
    // Check file reception in progress
    if (!_pReceivedFileDataPtr)
        return;

    // Get block location
    char blockStartStr[MAX_INT_ARG_STR_LEN+1];
    if (!jsonGetValueForKey("index", pCmdJson, blockStartStr, MAX_INT_ARG_STR_LEN))
        return;
    int blockStart = strtol(blockStartStr, NULL, 10);
    if (blockStart < 0)
        return;

    // LogWrite(FromCmdHandler, LOG_DEBUG, "efBlock, len %d, blocks %d %s", 
    //            _receivedFileBytesRx, _receivedBlockCount,
    //            (blockStart + dataLen > _receivedFileBufSize) ? "TOO LONG" : "");

    // Check if outside bounds of file length buffer
    if (blockStart + dataLen > _receivedFileBufSize)
        return;

    // Store the data
    memcpy(_pReceivedFileDataPtr+blockStart, pData, dataLen);
    _receivedFileBytesRx += dataLen;

    // Add to count of blocks
    _receivedBlockCount++;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File Reception - end
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::handleFileEnd(const char* pCmdJson)
{
    // Check file reception in progress
    if (!_pReceivedFileDataPtr)
        return;

    // Check block count
    char blockCountStr[MAX_INT_ARG_STR_LEN+1];
    if (!jsonGetValueForKey("blockCount", pCmdJson, blockCountStr, MAX_INT_ARG_STR_LEN))
        return;
    int blockCount = strtoul(blockCountStr, NULL, 10);
    char ackMsgJson[100];
    ee_sprintf(ackMsgJson, "\"rxCount\":%d, \"expCount\":%d", _receivedBlockCount, blockCount);
    if (blockCount != _receivedBlockCount)
    {
        LogWrite(FromCmdHandler, LOG_WARNING, "efEnd File %s, blockCount rx %d != sent %d", 
                _receivedFileName, _receivedBlockCount, blockCount);
        sendWithJSON("ufEndNotAck", ackMsgJson);
        return;
    }
    else
    {
        sendWithJSON("ufEndAck", ackMsgJson);
    }

    // See if any sockets want to handle this
    bool isFirmware = strcasecmp(_pReceivedFileType, "firmware") == 0;
    if (isFirmware)
    {
        // Short delay to allow comms completion 
        microsDelay(100000);
        LogWrite(FromCmdHandler, LOG_DEBUG, "efEnd IMG firmware update File %s, len %d", _receivedFileName, _receivedFileBytesRx);
    }
    else
    {
        // LogWrite(FromCmdHandler, LOG_VERBOSE, "efEnd File %s, len %d", _receivedFileName, _receivedFileBytesRx);
    }
    commsSocketHandleReceivedFile(_receivedFileStartInfo, _pReceivedFileDataPtr, _receivedFileBytesRx, isFirmware);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send key code to target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Send key code to target
void CommandHandler::sendKeyCodeToTarget(int keyCode)
{
    static const int MAX_KEY_CMD_STR_LEN = 100;
    static char keyStr[MAX_KEY_CMD_STR_LEN+1];
    strlcpy(keyStr, "{\"cmdName\":\"keyCode\",\"key\":", MAX_KEY_CMD_STR_LEN);
    rditoa(keyCode, (uint8_t*)(keyStr+strlen(keyStr)), 10, 10);
    strlcpy(keyStr+strlen(keyStr), "}", MAX_KEY_CMD_STR_LEN);
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->_miniHDLC.sendFrame((const uint8_t*)keyStr, strlen(keyStr)+1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send with JSON payload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::sendWithJSON(const char* cmdName, const char* cmdJson)
{
    // Form and send command
    static const int MAX_CMD_STR_LEN = 1500;
    static char cmdStr[MAX_CMD_STR_LEN+1];
    strlcpy(cmdStr, "{\"cmdName\":\"", MAX_CMD_STR_LEN);
    strlcpy(cmdStr+strlen(cmdStr), cmdName, MAX_CMD_STR_LEN);
    strlcpy(cmdStr+strlen(cmdStr), "\",", MAX_CMD_STR_LEN);
    strlcpy(cmdStr+strlen(cmdStr), cmdJson, MAX_CMD_STR_LEN);
    strlcpy(cmdStr+strlen(cmdStr), "}", MAX_CMD_STR_LEN);
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->_miniHDLC.sendFrame((const uint8_t*)cmdStr, strlen(cmdStr)+1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send an API request
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::sendAPIReq(const char* reqLine)
{
    // Form and send
    static const int MAX_REQ_STR_LEN = 100;
    static char reqStr[MAX_REQ_STR_LEN+1];
    strlcpy(reqStr, "\"req\":\"", MAX_REQ_STR_LEN);
    strlcpy(reqStr+strlen(reqStr), reqLine, MAX_REQ_STR_LEN);
    strlcpy(reqStr+strlen(reqStr), "\"", MAX_REQ_STR_LEN);
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->sendWithJSON("apiReq", reqStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send RDP message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::sendData(const char* cmdName, const uint8_t* pData, uint32_t len, uint32_t index)
{
    // LogWrite(FromCmdHandler, LOG_VERBOSE, "RDP replying with %s", pStr);
    static const int MAX_RESPONSE_MSG_LEN = 2000;
    static char dataFrame[MAX_RESPONSE_MSG_LEN+1];
    static char indexStr[10];
    itoa(index, indexStr, 10);
    static char lenStr[10];
    itoa(len, lenStr, 10);
    strlcpy(dataFrame, "{\"cmdName\":\"", MAX_RESPONSE_MSG_LEN);
    strlcat(dataFrame, cmdName, MAX_RESPONSE_MSG_LEN);
    strlcat(dataFrame, "\",\"index\":\"", MAX_RESPONSE_MSG_LEN);
    strlcat(dataFrame, indexStr, MAX_RESPONSE_MSG_LEN);
    strlcat(dataFrame, "\",\"len\":\"", MAX_RESPONSE_MSG_LEN);
    strlcat(dataFrame, lenStr, MAX_RESPONSE_MSG_LEN);
    strlcat(dataFrame, "\"}", MAX_RESPONSE_MSG_LEN);
    uint32_t dataFrameLen = strlen(dataFrame)+1+len+1;    
    if (dataFrameLen >= MAX_RESPONSE_MSG_LEN)
        return;
    memcpy(dataFrame+strlen(dataFrame)+1, pData, len);
    dataFrame[dataFrameLen] = 0;
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->_miniHDLC.sendFrame((const uint8_t*)dataFrame, dataFrameLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log debug message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::logDebugMessage(const char* pStr)
{
    static const int MAX_RESPONSE_MSG_LEN = 2000;
    char responseJson[MAX_RESPONSE_MSG_LEN+1];
    strlcpy(responseJson, "\"msg\":\"", MAX_RESPONSE_MSG_LEN);
    strlcat(responseJson, pStr, MAX_RESPONSE_MSG_LEN);
    strlcat(responseJson, "\"", MAX_RESPONSE_MSG_LEN);
    sendWithJSON("log", responseJson);
}

void CommandHandler::logDebugJson(const char* pStr)
{
    sendWithJSON("log", pStr);
}

void CommandHandler::logDebug(const char* pSeverity, const char* pSource, const char* pMsg)
{
    // Escape the message since it will be sent inside JSON
    static const int MAX_LOG_MSG_LEN = 5000;
    char escapedMsg[MAX_LOG_MSG_LEN];
    jsonEscape(pMsg, escapedMsg, MAX_LOG_MSG_LEN);

    // Form the message
    char logJson[MAX_LOG_MSG_LEN+1];
    strlcpy(logJson, "\"msg\":\"", MAX_LOG_MSG_LEN);
    strlcat(logJson, escapedMsg, MAX_LOG_MSG_LEN);
    strlcat(logJson, "\",", MAX_LOG_MSG_LEN);
    strlcat(logJson, "\"lev\":\"", MAX_LOG_MSG_LEN);
    strlcat(logJson, pSeverity, MAX_LOG_MSG_LEN);
    strlcat(logJson, "\",", MAX_LOG_MSG_LEN);
    strlcat(logJson, "\"src\":\"", MAX_LOG_MSG_LEN);
    strlcat(logJson, pSource, MAX_LOG_MSG_LEN);
    strlcat(logJson, "\"", MAX_LOG_MSG_LEN);
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->logDebugJson(logJson);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::service()
{
}
