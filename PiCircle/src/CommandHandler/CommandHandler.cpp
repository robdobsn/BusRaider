// Bus Raider
// Rob Dobson 2018-2019

#include "CommandHandler.h"
#include "../System/LogHandler.h"
#include "../System/JsonUtils.h"
#include "../System/StringUtils.h"
#include <stdlib.h>

static const char FromCommandHandler[] = "CommandHandler";

static const int CMD_HANDLER_MAX_CMD_STR_LEN = 200;

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
    LogWrite(FromCommandHandler, LogNotice, "Rx %d bytes, frames %d", bytesRx, totalFrames);

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

    // Handle commands
    if (strcasecmp(cmdName, "ufStart") == 0)
    {
        LogWrite(FromCommandHandler, LOG_NOTICE, "ufStart");
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
    // //     CLogger::Get()->Write(FromCommandHandler, LogNotice, "%d", charsRead);
    // numRead += charsRead;
    // totalRead += charsRead;
}
