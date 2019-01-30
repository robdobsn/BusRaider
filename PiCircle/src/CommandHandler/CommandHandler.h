// Bus Raider
// Rob Dobson 2018-2019
// Command handler

#pragma once
#include "MiniHDLC.h"
#include <stdint.h>

typedef void CmdHandlerChangeMachineFnType(const char* machineName);
typedef void CmdHandlerPutBytesFnType(const uint8_t* pBytes, int numBytes);

class CommandHandler
{
public:
    CommandHandler();
    ~CommandHandler();

    void setCallbacks(CmdHandlerPutBytesFnType* pPutBytesFunction, CmdHandlerChangeMachineFnType* pChangeMcFunction)
    {
        _pPutBytesFunction = pPutBytesFunction;
        _pChangeMcFunction = pChangeMcFunction;
    }

    void handleBuffer(const uint8_t* pBytes, int numBytes);

    void service();

    int totalFrames;
    int bytesRx;

private:
    static void static_hdlcPutCh(uint8_t ch);
    static void static_hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);
    void hdlcPutCh(uint8_t ch);
    void hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);
    void processCommand(const char* pCmdJson, const uint8_t* pParams, int paramsLen);

private:
    static CmdHandlerChangeMachineFnType* _pChangeMcFunction;
    static CmdHandlerPutBytesFnType* _pPutBytesFunction;
    static CommandHandler* _pSingletonCommandHandler;
    MiniHDLC _miniHDLC;

};
