// Bus Raider
// Rob Dobson 2018-2019

#include "CommandHandler.h"
#include "../System/LogHandler.h"
#include <stdlib.h>

static const char FromCommandHandler[] = "CommandHandler";

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

void CommandHandler::hdlcFrameRx(const uint8_t *frameBuffer, int frameLength)
{
    bytesRx += frameLength;
    totalFrames++;
    LogWrite(FromCommandHandler, LogNotice, "Rx %d bytes, frames %d", bytesRx, totalFrames);
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
