
#pragma once

#include "UartMaxiSerialDevice.h"
#include "MiniHDLC.h"
#include "CommandHandler.h"

class AppSerialIF;

class CommsManager
{
public:
	CommsManager (CUartMaxiSerialDevice* pSerial, AppSerialIF* pAppSerialIF);
	~CommsManager (void);

    // Setup
    bool setup();

    // Service - called frequently
    void service();

    // Send message
    void sendWithJSON(const char* cmdName, const char* cmdJson, uint32_t msgIdx, 
            const uint8_t* pData, uint32_t dataLen);

private:
    // Singleton pointer
    static CommsManager* _pCommsManager;

    // Comms socket
    static CommsSocketInfo _commsSocket;

    // CommandHandler
    CommandHandler _commandHandler;	

    // Serial device
    CUartMaxiSerialDevice* _pSerial;

    // AppSerialIF
    AppSerialIF* _pAppSerialIF;
    
	// Service getting from serial
    void serviceGetFromSerial();

	// Callbacks
    static void hdlcFrameRxCBStatic(const uint8_t* pFrame, unsigned frameLen);
    void hdlcFrameRxCB(const uint8_t* pFrame, unsigned frameLen);
    static void hdlcFrameTxCBStatic(const uint8_t* pFrame, unsigned frameLen);
    void hdlcFrameTxCB(const uint8_t* pFrame, unsigned frameLen);

    // Helpers
	static void serialPutStr(const uint8_t* pBuf, unsigned len);
    static uint32_t serialTxAvailable();
	static bool handleRxMsg(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen);
	static bool performUpdate(const uint8_t* pData, unsigned dataLen);
};

