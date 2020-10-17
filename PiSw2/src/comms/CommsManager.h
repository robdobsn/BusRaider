
#pragma once

#include "UartMaxiSerialDevice.h"
#include "CommandHandler.h"

class AppSerialIF;

class CommsManager
{
public:
	CommsManager (CUartMaxiSerialDevice& serial, AppSerialIF* pAppSerialIF);
	~CommsManager (void);

    // Setup
    bool setup();

    // Service - called frequently
    void service();

    // Send message
    void sendWithJSON(const char* cmdName, const char* cmdJson, uint32_t msgIdx, 
            const uint8_t* pData, uint32_t dataLen);

    // Get command handler
    CommandHandler& getCommandHandler()
    {
        return _commandHandler;
    }
    
    // OTA
    bool otaIsPending()
    {
        return _otaBufLen > 0;
    }
    uint8_t* otaBuffer()
    {
        return _pOTABuffer;
    }
    uint32_t otaBufLen()
    {
        return _otaBufLen;
    }
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

    // Pending OTA
    uint8_t* _pOTABuffer;
    uint32_t _otaBufLen;
    
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
	static bool handleRxMsg(void* pObject, const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen);
	static bool registerPendingOTA(const uint8_t* pData, unsigned dataLen);
};

