
#include "CommandHandler.h"
#include <stdlib.h>
#include <circle/logger.h>

static const char FromCommandHandler[] = "CommandHandler";

// Callback for change machine type
CmdHandlerChangeMachineFnType CommandHandler::_pChangeMcFunction = NULL;
CmdHandlerRxCharFnType CommandHandler::_pRxCharFunction = NULL;

// Singleton command handler
CommandHandler* CommandHandler::_pSingletonCommandHandler = NULL;

// Constructor
CommandHandler::CommandHandler(CSerialDevice& serialConn, CmdHandlerChangeMachineFnType pChangeMcFunction) :
    _serialConn(serialConn),
    _miniHDLC(static_hdlcPutCh, static_hdlcFrameRx)
{   
    _pChangeMcFunction = pChangeMcFunction;
    _pSingletonCommandHandler = this;




    totalRead = 0;
    numRead = 0;
    bytesRx = 0;
}

CommandHandler::~CommandHandler()
{
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
    uint8_t buf[1];
    buf[0] = ch;
    _serialConn.Write(buf, 1);
}

void CommandHandler::hdlcFrameRx(const uint8_t *frameBuffer, int frameLength)
{
    bytesRx += frameLength;
    // CLogger::Get()->Write(FromCommandHandler, LogNotice, "HDLC frame received, len %d total %d", frameLength, bytesRx);
}

void CommandHandler::service()
{
    static const int MAX_CHARS_TO_READ = 10000;
    unsigned char buf[MAX_CHARS_TO_READ];
    int bytesRead = _serialConn.Read(buf, MAX_CHARS_TO_READ);
    numRead += bytesRead;
    totalRead += bytesRead;
    _miniHDLC.handleBuffer(buf, bytesRead);

    // uint8_t buf[MAX_CHARS_TO_READ];
    // int charsRead = _serialConn.Read(buf, MAX_CHARS_TO_READ);
    // // for (int i = 0; i < charsRead; i++)
    // //     _miniHDLC.handleChar(buf[i]);
    // // if (charsRead > 0)
    // //     CLogger::Get()->Write(FromCommandHandler, LogNotice, "%d", charsRead);
    // numRead += charsRead;
    // totalRead += charsRead;
}
