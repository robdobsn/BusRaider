// Bus Raider
// Rob Dobson 2018
// Command handler

#pragma once
#include "MiniHDLC.h"
#include <circle/serial.h>
#include <stdint.h>

typedef void (*CmdHandlerChangeMachineFnType) (const char* machineName);
typedef void (*CmdHandlerRxCharFnType) (const uint8_t* rxChars, int rxLen);

class CommandHandler
{
public:
    CommandHandler(CSerialDevice& serialConn, CmdHandlerChangeMachineFnType changeMcFunction);
    ~CommandHandler();

    void setRxCharCallback(CmdHandlerRxCharFnType rxCharFunction)
    {
        _pRxCharFunction = rxCharFunction;
    }

    void service();

    int totalRead;
    int numRead;
    int bytesRx;

private:
    static void static_hdlcPutCh(uint8_t ch);
    static void static_hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);
    void hdlcPutCh(uint8_t ch);
    void hdlcFrameRx(const uint8_t *frameBuffer, int frameLength);

private:
    static CmdHandlerChangeMachineFnType _pChangeMcFunction;
    static CmdHandlerRxCharFnType _pRxCharFunction;
    static CommandHandler* _pSingletonCommandHandler;
    CSerialDevice& _serialConn;
    MiniHDLC _miniHDLC;

};
