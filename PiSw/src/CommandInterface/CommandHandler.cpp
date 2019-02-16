// Bus Raider
// Rob Dobson 2018-2019

#include "CommandHandler.h"
#include "../System/nmalloc.h"
#include "../System/ee_printf.h"
#include "../TargetBus/TargetState.h"
#include "minihdlc.h"
#include "../FileFormats/srecparser.h"
#include "../Machines/McManager.h"
#include "../System/lowlev.h"
#include "../Utils/rdutils.h"
#include "../System/OTAUpdate.h"
#include <string.h>
#include <stdlib.h>
#include "../Utils/rdstdcpp.h"

// Module name
static const char FromCmdHandler[] = "CommandHandler";

// Callback for change machine type
CmdHandlerChangeMachineFnType* CommandHandler::_pChangeMcFunction = NULL;
CmdHandlerRxFromTargetFnType* CommandHandler::_pRxFromTargetFunction = NULL;
CmdHandlerPutToSerialFnType* CommandHandler::_pPutToSerialFunction = NULL;

// Singleton command handler
CommandHandler* CommandHandler::_pSingletonCommandHandler = NULL;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
CommandHandler::CommandHandler() :
    _miniHDLC(static_hdlcPutCh, static_hdlcFrameRx)
{   
    // Callbacks
    _pRxFromTargetFunction = NULL;
    _pChangeMcFunction = NULL;
    _pSingletonCommandHandler = this;

    // File reception
    _receivedFileName[0] = 0;
    _pReceivedFileType[0] = 0;
    _receivedFileStartInfo[0] = 0;
    _pReceivedFileDataPtr = 0;
    _receivedFileBufSize = 0;
    _receivedFileBytesRx = 0;
    _receivedBlockCount = 0;

    // Status information for ESP32
    _statusIPAddress[0] = 0;
    _statusWifiConnStr[0] = 0;
    _statusWifiSSID[0] = 0;
    _statusIPAddressValid = false;

    // RDP index value str
    _lastRDPIndexValStr[0] = 0;
}

CommandHandler::~CommandHandler()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDLC interface functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::handleSerialReceivedChars(const uint8_t* pBytes, int numBytes)
{
    _miniHDLC.handleBuffer(pBytes, numBytes);
}

void CommandHandler::static_hdlcFrameRx(const uint8_t *frameBuffer, int frameLength)
{
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->hdlcFrameRx(frameBuffer, frameLength);
}

void CommandHandler::static_hdlcPutCh(uint8_t ch)
{
    // LogWrite(FromCmdHandler, LOG_DEBUG, "static_hdlcPutCh");

    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->hdlcPutCh(ch);
}

void CommandHandler::hdlcPutCh(uint8_t ch)
{
    if (_pPutToSerialFunction)
    {
        uint8_t buf[1];
        buf[0] = ch;
        _pPutToSerialFunction(buf, 1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HDLC frame handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle frame received from HDLC
// This is a ascii character string (null terminated) followed by a byte buffer containing parameters
void CommandHandler::hdlcFrameRx(const uint8_t *pFrame, int frameLength)
{
    // LogWrite(FromCmdHandler, LOG_DEBUG, "Rx %d bytes", frameLength);

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
    #define MAX_CMD_NAME_STR 30
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return;

    // LogWrite(FromCmdHandler, LOG_NOTICE, "processCommand JSON %s cmdName %s", pCmdJson, cmdName); 

    // Handle commands
    if (strcasecmp(cmdName, "ufStart") == 0)
    {
        LogWrite(FromCmdHandler, LOG_NOTICE, "processCommand fileStart"); 
        handleFileStart(pCmdJson);
    }
    else if (strcasecmp(cmdName, "ufBlock") == 0)
    {
        handleFileBlock(pCmdJson, pParams, paramsLen);
    }
    else if (strcasecmp(cmdName, "ufEnd") == 0)
    {
        handleFileEnd(pCmdJson);
    }
    else if (strcasecmp(cmdName, "ClearTarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "ClearTarget");
        TargetState::clear();
    }
    else if ((strcasecmp(cmdName, "ProgramAndReset") == 0) || (strcasecmp(cmdName, "ProgramTarget") == 0))
    {
        McManager::handleTargetProgram(cmdName);
    }
    else if (strcasecmp(cmdName, "ResetTarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "ResetTarget");
        McManager::targetReset();
    }
    else if (strcasecmp(cmdName, "IOClrTarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "IO Clear Target");
        McManager::targetClearAllIO();
    }
    else if (strcasecmp(cmdName, "FileTarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "File to Target, len %d", paramsLen);
        McBase* pMc = McManager::getMachine();
        if (pMc)
            pMc->fileHandler(pCmdJson, pParams, paramsLen);
    }
    else if (strcasecmp(cmdName, "SRECTarget") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "SREC to Target, len %d", paramsLen);
        srec_decode(TargetState::addMemoryBlock, addressRxCallback, pParams, paramsLen);
    }
    else if (strncasecmp(cmdName, "SetMachine", strlen("SetMachine")) == 0)
    {
        // Get machine name
        const char* pMcName = strstr(cmdName,"=");
        if (pMcName)
        {
            // Move to first char of actual name
            pMcName++;
            if (_pChangeMcFunction != NULL)
                _pChangeMcFunction(pMcName);
            LogWrite(FromCmdHandler, LOG_DEBUG, "Set Machine to %s", pMcName);
        }
    }
    else if (strcasecmp(cmdName, "RxHost") == 0)
    {
        // LogWrite(FromCmdHandler, LOG_DEBUG, "RxFromHost, len %d", dataLen);
        if (_pRxFromTargetFunction)
            _pRxFromTargetFunction(pParams, paramsLen);
    }
    else if (strcasecmp(cmdName, "respMsg") == 0)
    {
        // Handle status response message
        handleStatusResponse(pCmdJson);
    }
    else if (strcasecmp(cmdName, "rdp") == 0)
    {
        // Get message index value
        strcpy(_lastRDPIndexValStr, "0");
        if (!jsonGetValueForKey("index", pCmdJson, _lastRDPIndexValStr, MAX_RDP_INDEX_VAL_LEN))
            LogWrite(FromCmdHandler, LOG_DEBUG, "RDP NO INDEX VAL");
        // Send to remote debug handler
        static const int MAX_CMD_STR_LEN = 200;
        static char commandStr[MAX_CMD_STR_LEN+1];
        if (paramsLen > MAX_CMD_STR_LEN)
            paramsLen = MAX_CMD_STR_LEN;
        memcpy(commandStr, pParams, paramsLen);
        commandStr[paramsLen] = 0;
        static const int MAX_RESPONSE_MSG_LEN = 2000;
        static char responseMessage[MAX_RESPONSE_MSG_LEN+1];
        responseMessage[0] = 0;
        McManager::debuggerCommand(commandStr, responseMessage, MAX_RESPONSE_MSG_LEN);
        // Send response back
        sendDebugMessage(responseMessage);
    }
    else
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "Unknown command %s", cmdName);
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

    LogWrite(FromCmdHandler, LOG_NOTICE, "ufStart File %s, toPtr %08x", 
                _receivedFileName, _pReceivedFileDataPtr);

    // Get file type
    if (!jsonGetValueForKey("fileType", pCmdJson, _pReceivedFileType, MAX_FILE_TYPE_STR))
        return;

    LogWrite(FromCmdHandler, LOG_NOTICE, "ufStart FileType %s", 
                _receivedFileName);

    // Get file length
    char fileLenStr[MAX_INT_ARG_STR_LEN+1];
    if (!jsonGetValueForKey("fileLen", pCmdJson, fileLenStr, MAX_INT_ARG_STR_LEN))
        return;

    LogWrite(FromCmdHandler, LOG_NOTICE, "ufStart FileLenStr %s", 
                fileLenStr);

    int fileLen = strtol(fileLenStr, NULL, 10);
    if (fileLen <= 0)
        return;
    LogWrite(FromCmdHandler, LOG_NOTICE, "ufStart FileLen %d", 
                fileLen);

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
        LogWrite(FromCmdHandler, LOG_NOTICE, "efStart File %s, toPtr %08x, bufSize %d", 
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

    // Check if outside bounds of file length buffer
    if (blockStart + dataLen > _receivedFileBufSize)
        return;

    // Store the data
    memcpy(_pReceivedFileDataPtr+blockStart, pData, dataLen);
    _receivedFileBytesRx += dataLen;

    // Add to count of blocks
    _receivedBlockCount++;
    LogWrite(FromCmdHandler, LOG_DEBUG, "efBlock, len %d, blocks %d", 
                _receivedFileBytesRx, _receivedBlockCount);
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
    if (blockCount != _receivedBlockCount)
    {
        LogWrite(FromCmdHandler, LOG_WARNING, "efEnd File %s, blockCount rx %d != sent %d", 
                _receivedFileName, _receivedBlockCount, blockCount);
        return;
    }

    // Check file type
    if (strcasecmp(_pReceivedFileType, "firmware") == 0)
    {
        LogWrite(FromCmdHandler, LOG_DEBUG, "efEnd IMG firmware update File %s, len %d", _receivedFileName, _receivedFileBytesRx);
        OTAUpdate::performUpdate(_pReceivedFileDataPtr, _receivedFileBytesRx);
    }
    else
    {
        // Offer to the machine specific handler
        // appendJson(_receivedFileStartInfo, pCmdJson);
        LogWrite(FromCmdHandler, LOG_DEBUG, "efEnd File %s, len %d", _receivedFileName, _receivedFileBytesRx);
        McBase* pMc = McManager::getMachine();
        if (pMc)
            pMc->fileHandler(_receivedFileStartInfo, _pReceivedFileDataPtr, _receivedFileBytesRx);

    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address callback
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::handleStatusResponse(const char* pCmdJson)
{
    // LogWrite(FromCmdHandler, LOG_DEBUG, "RxFrame RespMsg");
    // Get espHealth field
    char espHealthJson[MAX_ESP_HEALTH_STR];
    if (!jsonGetValueForKey("espHealth", pCmdJson, espHealthJson, MAX_ESP_HEALTH_STR))
        return;
    if (!jsonGetValueForKey("wifiIP", espHealthJson, _statusIPAddress, MAX_IP_ADDR_STR+1))
        return;
    if (!jsonGetValueForKey("wifiConn", espHealthJson, _statusWifiConnStr, MAX_WIFI_CONN_STR+1))
        return;
    if (!jsonGetValueForKey("ssid", espHealthJson, _statusWifiSSID, MAX_WIFI_SSID_STR+1))
        return;
    // LogWrite(FromCmdHandler, LOG_DEBUG, "Ip Address %s", _espIPAddress);
    _statusIPAddressValid = (strcmp(_statusIPAddress, "0.0.0.0") != 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Address callback from decode function
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::addressRxCallback(uint32_t addr)
{
    LogWrite(FromCmdHandler, LOG_DEBUG, "CmdHandler: got addr from SREC %04x", addr);
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
    _miniHDLC.sendFrame((const uint8_t*)keyStr, strlen(keyStr)+1);
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
    _miniHDLC.sendFrame((const uint8_t*)cmdStr, strlen(cmdStr)+1);
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
    strlcpy(reqStr+strlen(reqStr), "\",", MAX_REQ_STR_LEN);
    sendWithJSON("apiReq", reqStr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status response
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::requestStatusUpdate()
{
    // Check if file transfer in progress
    if (_pReceivedFileDataPtr)
        return;

    // Send update
    const char* mcJSON = McManager::getMachineJSON();
    sendWithJSON("statusUpdate", mcJSON);
    // Request update
    sendAPIReq("queryESPHealth");    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status response
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::getStatusResponse(bool* pIPAddressValid, char** pIPAddress, char** pWifiConnStr, char** pWifiSSID)
{
    *pIPAddressValid = _statusIPAddressValid;
    *pIPAddress = _statusIPAddress;
    *pWifiConnStr = _statusWifiConnStr;
    *pWifiSSID = _statusWifiSSID;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send debug message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::sendDebugMessage(const char* pStr)
{
    // LogWrite(FromCmdHandler, LOG_DEBUG, "RDP replying with %s", responseMessage);
    static const int MAX_RESPONSE_MSG_LEN = 2000;
    static char responseJson[MAX_RESPONSE_MSG_LEN+1];
    strlcpy(responseJson, "\"index\":\"", MAX_RESPONSE_MSG_LEN);
    strlcat(responseJson, _lastRDPIndexValStr, MAX_RESPONSE_MSG_LEN);
    strlcat(responseJson, "\",\"content\":\"", MAX_RESPONSE_MSG_LEN);
    strlcat(responseJson, pStr, MAX_RESPONSE_MSG_LEN);
    strlcat(responseJson, "\"", MAX_RESPONSE_MSG_LEN);
    sendWithJSON("rdp", responseJson);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandHandler::service()
{
}
