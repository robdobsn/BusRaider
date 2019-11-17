// Bus Raider
// Rob Dobson 2018-2019

#include "CommandHandler.h"
#include "../System/nmalloc.h"
#include "../System/ee_sprintf.h"
#include "../System/logging.h"
#include "MiniHDLC.h"
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
CmdHandlerSerialPutStrFnType* CommandHandler::_pHDLCSerialPutStrFunction = NULL;
CmdHandlerSerialTxAvailableFnType* CommandHandler::_pHDLCSerialTxAvailableFunction = NULL;

// Singleton command handler
CommandHandler* CommandHandler::_pSingletonCommandHandler = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
CommandHandler::CommandHandler() :
    _miniHDLC(hdlcPutChStatic, hdlcFrameRxStatic),
    _usbKeyboardRingBufPos(MAX_USB_KEYBOARD_CHARS)
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

    // TODO
    // _rdpMsgCountIn = 0;
    // _rdpMsgCountOut = 0;
    // _rdpTimeUs = 0;
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
    if (_pHDLCSerialPutStrFunction)
    {
        uint8_t buf[1];
        buf[0] = ch;
        _pHDLCSerialPutStrFunction(buf, 1);
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

uint32_t CommandHandler::hdlcTxAvailableStatic()
{
    if (_pSingletonCommandHandler)
        if (_pSingletonCommandHandler->_pHDLCSerialTxAvailableFunction)
            return _pSingletonCommandHandler->_pHDLCSerialTxAvailableFunction();
    return 0;
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
    static const int MAX_MSGIDX_STR_LEN = 20;
    char msgIdxStr[MAX_MSGIDX_STR_LEN];
    msgIdxStr[0] = 0;
    jsonGetValueForKey("msgIdx", pCmdJson, msgIdxStr, MAX_MSGIDX_STR_LEN);

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
        // Rdp msg idx is the outer msgIdx
        rdpIndex = strtol(msgIdxStr, NULL, 10);

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
        jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR);
        jsonGetValueForKey("msgIdx", pCmdJson, msgIdxStr, MAX_MSGIDX_STR_LEN);
        rdpMessage = true;

        // TODO
        // if (strcasecmp(cmdName, "tracerStatus") == 0)
        //     _rdpMsgCountIn++;
        // LogWrite(FromCmdHandler, LOG_DEBUG, "RDPRX cmd %s cmdLen %d paramsStr %s paramslen %d rdCountIn %d this %d", 
        //             pCmdJson, strlen(pCmdJson), pParams, paramsLen, _rdpMsgCountIn, this);
    }
              
    // Offer to comms sockets
    char respJson[MAX_DATAFRAME_LEN];
    respJson[0] = 0;
    commsSocketHandleRxMsg(pCmdJson, pParams, paramsLen, respJson, MAX_DATAFRAME_LEN);

    // if (rdpMessage)
    //     LogWrite(FromCmdHandler, LOG_DEBUG, "CMDMSG rx cmd %s cmdLen %d paramsStr %s paramslen %d respJson %s", 
    //             pCmdJson, strlen(pCmdJson), pParams, paramsLen, respJson);

    // Form response cmdName
    char cmdNameResp[MAX_CMD_NAME_STR];
    strlcpy(cmdNameResp, cmdName, MAX_CMD_NAME_STR);
    strlcat(cmdNameResp, "Resp", MAX_CMD_NAME_STR);

    if ((rdpMessage) && (strlen(respJson) > 0))
    {
        // Wrap up the JSON
        static const int JSON_RESP_MAX_LEN = 10000;
        char jsonFrame[JSON_RESP_MAX_LEN];
        strlcpy(jsonFrame, "{\"cmdName\":\"", JSON_RESP_MAX_LEN);
        strlcat(jsonFrame, cmdNameResp, JSON_RESP_MAX_LEN);
        strlcat(jsonFrame, "\"", JSON_RESP_MAX_LEN);
        strlcat(jsonFrame, ",", JSON_RESP_MAX_LEN);
        strlcat(jsonFrame, respJson, JSON_RESP_MAX_LEN);
        if (strlen(msgIdxStr) > 0)
        {
            strlcat(jsonFrame, ",\"msgIdx\":", JSON_RESP_MAX_LEN);
            strlcat(jsonFrame, msgIdxStr, JSON_RESP_MAX_LEN);
        }
        strlcat(jsonFrame, ",\"dataLen\":0}", JSON_RESP_MAX_LEN);

        // Send response back
        sendWithJSON("rdp", "", rdpIndex, (const uint8_t*)jsonFrame, strlen(jsonFrame));
    }
    else
    {
        if (strlen(respJson) > 0)
            sendWithJSON(cmdNameResp, respJson);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms socket handlers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::commsSocketHandleRxMsg(const char* pCmdJson, const uint8_t* pParams, int paramsLen,
                    char* pRespJson, int maxRespLen)
{
    if (_commsSocketCount < 3)
        LogWrite(FromCmdHandler, LOG_DEBUG, "RxMsg fewer sockets than expected %d", _commsSocketCount);
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

    // LogWrite(FromCmdHandler, LOG_DEBUG, "ufStart File %s, toPtr %08x", 
    //             _receivedFileName, _pReceivedFileDataPtr);

    // Get file type
    if (!jsonGetValueForKey("fileType", pCmdJson, _pReceivedFileType, MAX_FILE_TYPE_STR))
        return;

    // LogWrite(FromCmdHandler, LOG_DEBUG, "ufStart FileType %s", 
    //             _receivedFileName);

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
        // LogWrite(FromCmdHandler, LOG_DEBUG, "ufStart File %s, toPtr %08x, bufSize %d", 
        //             _receivedFileName, _pReceivedFileDataPtr, _receivedFileBufSize);
    }
    else
    {
        _receivedFileBufSize = 0;
        LogWrite(FromCmdHandler, LOG_WARNING, "ufStart unable to allocate memory for file %s, bufSize %d", 
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
    memcopyfast(_pReceivedFileDataPtr+blockStart, pData, dataLen);
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
        LogWrite(FromCmdHandler, LOG_WARNING, "ufEnd File %s, blockCount rx %d != sent %d", 
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
        LogWrite(FromCmdHandler, LOG_DEBUG, "ufEnd IMG firmware update File %s, len %d", _receivedFileName, _receivedFileBytesRx);
    }
    else
    {
        // LogWrite(FromCmdHandler, LOG_VERBOSE, "efEnd File %s, len %d", _receivedFileName, _receivedFileBytesRx);
    }
    commsSocketHandleReceivedFile(_receivedFileStartInfo, _pReceivedFileDataPtr, _receivedFileBytesRx, isFirmware);
    LogWrite(FromCmdHandler, LOG_DEBUG, "ufEnd File %s, len %d Completed", _receivedFileName, _receivedFileBytesRx);
    
    // File handling completed
    _pReceivedFileDataPtr = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File receive status
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandHandler::getFileReceiveStatus(uint32_t& fileLen, uint32_t& filePos)
{
    if ((_receivedFileBufSize == 0) || (_receivedFileBufSize == _receivedFileBytesRx))
        return false;
    fileLen = _receivedFileBufSize;
    filePos = _receivedFileBytesRx;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send key code to target
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Send key code to target
void CommandHandler::sendKeyCodeToTargetStatic(int keyCode)
{
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->sendKeyCodeToTarget(keyCode);
}

void CommandHandler::sendKeyCodeToTarget(int keyCode)
{
    // Place in ring buffer
    if (_pSingletonCommandHandler->_usbKeyboardRingBufPos.canPut())
    {
        _usbKeyboardRingBuffer[_usbKeyboardRingBufPos.posToPut()] = keyCode;
        _usbKeyboardRingBufPos.hasPut();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check if we can send
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t CommandHandler::getTxAvailable()
{
    return hdlcTxAvailableStatic();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send with JSON payload
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::sendWithJSON(const char* cmdName, const char* cmdJson, uint32_t msgIdx, 
            const uint8_t* pData, uint32_t dataLen)
{
    // Form and send command
    // This takes the form of a binary buffer, the first part is a JSON string null terminated
    // This JSON contains a dataLen value which determines the length of the binary part that follows
    // A pure binary part immediately follows the null terminator of the JSON
    // It is also null terminated and the null terminator is not included in the dataLan value
    static const int MAX_DATAFRAME_LEN = 10000;
    char dataFrame[MAX_DATAFRAME_LEN];
    char indexStr[10];
    itoa(msgIdx, indexStr, 10);
    char lenStr[10];
    itoa(dataLen, lenStr, 10);
    strlcpy(dataFrame, "{\"cmdName\":\"", MAX_DATAFRAME_LEN);
    strlcat(dataFrame, cmdName, MAX_DATAFRAME_LEN);
    strlcat(dataFrame, "\"", MAX_DATAFRAME_LEN);
    if (strlen(cmdJson) > 0)
    {
        strlcat(dataFrame, ",", MAX_DATAFRAME_LEN);
        strlcat(dataFrame, cmdJson, MAX_DATAFRAME_LEN);
    }
    strlcat(dataFrame, ",\"msgIdx\":", MAX_DATAFRAME_LEN);
    strlcat(dataFrame, indexStr, MAX_DATAFRAME_LEN);
    strlcat(dataFrame, ",\"dataLen\":", MAX_DATAFRAME_LEN);
    strlcat(dataFrame, lenStr, MAX_DATAFRAME_LEN);
    strlcat(dataFrame, "}", MAX_DATAFRAME_LEN);
    // Allow for two terminators (one after JSON and one at end of binary section)
    uint32_t dataFrameBinaryPos = strlen(dataFrame)+1;
    uint32_t dataFrameTotalLen = dataFrameBinaryPos+dataLen+1; 
    // LogWrite(FromCmdHandler, LOG_DEBUG, "SEND DATA cmd %s dataFr %s dataFrameLen %d tooLong %d msg %s",
    //             cmdName, dataFrame, dataFrameLen, dataFrameLen >= MAX_DATAFRAME_LEN, (pData ? pData : ""));
    if (dataFrameTotalLen >= MAX_DATAFRAME_LEN)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "Frame too long");
        return;
    }
    if (dataLen > 0)
    {
        if (pData)
            memcopyfast(dataFrame+dataFrameBinaryPos, pData, dataLen);
        else
            return;
    }
    // Terminate the binary portion too (belt-and-braces!)
    dataFrame[dataFrameTotalLen-1] = 0;
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->_miniHDLC.sendFrame((const uint8_t*)dataFrame, dataFrameTotalLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send an API request
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::sendAPIReq(const char* reqLine)
{
    // Form and send
    static const int MAX_REQ_STR_LEN = 100;
    char reqStr[MAX_REQ_STR_LEN+1];
    strlcpy(reqStr, "\"req\":\"", MAX_REQ_STR_LEN);
    strlcpy(reqStr+strlen(reqStr), reqLine, MAX_REQ_STR_LEN);
    strlcpy(reqStr+strlen(reqStr), "\"", MAX_REQ_STR_LEN);
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->sendWithJSON("apiReq", reqStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Log debug message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::logDebugMessage(const char* pStr)
{
    char responseJson[MAX_DATAFRAME_LEN+1];
    strlcpy(responseJson, "\"msg\":\"", MAX_DATAFRAME_LEN);
    strlcat(responseJson, pStr, MAX_DATAFRAME_LEN);
    strlcat(responseJson, "\"", MAX_DATAFRAME_LEN);
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
    // Check for checks to send to target
    if (_usbKeyboardRingBufPos.canGet())
    {
        int keyCode = _usbKeyboardRingBuffer[_usbKeyboardRingBufPos.posToGet()];
        _usbKeyboardRingBufPos.hasGot();
        const int MAX_KEY_CMD_STR_LEN = 100;
        char keyStr[MAX_KEY_CMD_STR_LEN];
        ee_sprintf(keyStr, "{\"cmdName\":\"keyCode\",\"key\":%d}", keyCode);
        _miniHDLC.sendFrame((const uint8_t*)keyStr, strlen(keyStr)+1);
    }
}
