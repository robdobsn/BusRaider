// Bus Raider
// Rob Dobson 2018-2019

#include "CommandHandler.h"
#include "../System/LogHandler.h"
#include "../System/JsonUtils.h"
#include "../System/StringUtils.h"
#include "../System/OTAUpdate.h"
#include <stdlib.h>

static const char FromCmdHandler[] = "CommandHandler";

// Callback for change machine type
CmdHandlerChangeMachineFnType* CommandHandler::_pChangeMcFunction = NULL;
CmdHandlerPutBytesFnType* CommandHandler::_pPutBytesFunction = NULL;

// Singleton command handler
CommandHandler* CommandHandler::_pSingletonCommandHandler = NULL;

// Constructor
CommandHandler::CommandHandler() :
    _miniHDLC(static_hdlcPutCh, static_hdlcFrameRx)
{   
    _pPutBytesFunction = NULL;
    _pChangeMcFunction = NULL;
    _pSingletonCommandHandler = this;

    // File reception
    _pReceivedFileDataPtr = 0;
    _receivedFileBufSize = 0;
    _receivedFileBytesRx = 0;
    _receivedBlockCount = 0;

    totalFrames = 0;
    bytesRx = 0;
}

CommandHandler::~CommandHandler()
{
}

void CommandHandler::handleBuffer(const uint8_t* pBytes, int numBytes)
{
    _miniHDLC.handleBuffer(pBytes, numBytes);
}

void CommandHandler::static_hdlcPutCh(uint8_t ch)
{
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->hdlcPutCh(ch);
}

void CommandHandler::static_hdlcFrameRx(const uint8_t *frameBuffer, int frameLength)
{
    if (_pSingletonCommandHandler)
        _pSingletonCommandHandler->hdlcFrameRx(frameBuffer, frameLength);
}

void CommandHandler::hdlcPutCh(uint8_t ch)
{
    if (_pPutBytesFunction)
    {
        uint8_t buf[1];
        buf[0] = ch;
        _pPutBytesFunction(buf, 1);
    }
}

// Handle frame received from HDLC
// This is a ascii character string (null terminated) followed by a byte buffer containing parameters
void CommandHandler::hdlcFrameRx(const uint8_t *pFrame, int frameLength)
{
    bytesRx += frameLength;
    totalFrames++;
    LogWrite(FromCmdHandler, LogNotice, "Rx %d bytes, frames %d", bytesRx, totalFrames);

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

// Process split-up command string and parameters
void CommandHandler::processCommand(const char* pCmdJson, const uint8_t* pParams, int paramsLen)
{
    // Get the command string from JSON
    #define MAX_CMD_NAME_STR 30
    char cmdName[MAX_CMD_NAME_STR+1];
    if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
        return;

    LogWrite(FromCmdHandler, LOG_NOTICE, "processCommand JSON %s cmdName %s", pCmdJson, cmdName); 

    // Handle commands
    if (strcasecmp(cmdName, "ufStart") == 0)
    {
        LogWrite(FromCmdHandler, LOG_NOTICE, "processCommand fileStart"); 
        handleFileStart(pCmdJson, pParams, paramsLen);
    }
    else if (strcasecmp(cmdName, "ufBlock") == 0)
    {
        handleFileBlock(pCmdJson, pParams, paramsLen);
    }
    else if (strcasecmp(cmdName, "ufEnd") == 0)
    {
        handleFileEnd(pCmdJson, pParams, paramsLen);
    }

}

// Handle reception of file start block
void CommandHandler::handleFileStart(const char* pCmdJson, const uint8_t* pParams, int paramsLen)
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

    int fileLen = strtoul(fileLenStr, NULL, 10);
    if (fileLen <= 0)
        return;
    LogWrite(FromCmdHandler, LOG_NOTICE, "ufStart FileLen %d", 
                fileLen);

    // Copy start info
    strlcpy(_receivedFileStartInfo, pCmdJson, CMD_HANDLER_MAX_CMD_STR_LEN);

    // Allocate space for file
    if (_pReceivedFileDataPtr != NULL)
        delete [] _pReceivedFileDataPtr;
    _pReceivedFileDataPtr = new uint8_t[fileLen];
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

void CommandHandler::handleFileBlock(const char* pCmdJson, const uint8_t* pData, int dataLen)
{
    // Check file reception in progress
    if (!_pReceivedFileDataPtr)
        return;

    // Get block location
    char blockStartStr[MAX_INT_ARG_STR_LEN+1];
    if (!jsonGetValueForKey("index", pCmdJson, blockStartStr, MAX_INT_ARG_STR_LEN))
        return;
    int blockStart = strtoul(blockStartStr, NULL, 10);
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

void CommandHandler::handleFileEnd(const char* pCmdJson, const uint8_t* pData, int dataLen)
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
        // McBase* pMc = McManager::getMachine();
        // if (pMc)
        //     pMc->fileHandler(_receivedFileStartInfo, _pReceivedFileDataPtr, _receivedFileBytesRx);

    }
}

void CommandHandler::service()
{
    // static const int MAX_CHARS_TO_READ = 10000;
    // unsigned char buf[MAX_CHARS_TO_READ];
    // int bytesRead = _serialConn.Read(buf, MAX_CHARS_TO_READ);
    // numRead += bytesRead;
    // totalRead += bytesRead;
    // _miniHDLC.handleBuffer(buf, bytesRead);

    // uint8_t buf[MAX_CHARS_TO_READ];
    // int charsRead = _serialConn.Read(buf, MAX_CHARS_TO_READ);
    // // for (int i = 0; i < charsRead; i++)
    // //     _miniHDLC.handleChar(buf[i]);
    // // if (charsRead > 0)
    // //     CLogger::Get()->Write(FromCmdHandler, LogNotice, "%d", charsRead);
    // numRead += charsRead;
    // totalRead += charsRead;
}
