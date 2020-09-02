#include "CommsManager.h"
#include "AppSerialIF.h"
#include <circle/logger.h>
#include <circle/chainboot.h>
#include "logging.h"

static const char* MODULE_PREFIX = "CommsManager";

CommsManager* CommsManager::_pCommsManager = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Comms socket
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CommsManager::CommsManager(CUartMaxiSerialDevice* pSerial, AppSerialIF* pAppSerialIF) :
    _pSerial(pSerial), _pAppSerialIF(pAppSerialIF)
{
    // Singleton
    _pCommsManager = this;

	// Setup command handler
	_commandHandler.setPutToSerialCallback(serialPutStr, serialTxAvailable);

    // Add a comms socket (should have index 0)
    _commandHandler.commsSocketAdd(this, true, handleRxMsg, performUpdate, NULL);
}

CommsManager::~CommsManager()
{
}

bool CommsManager::setup()
{
    return true;
}

void CommsManager::service()
{
    // Service commmand handler
    _commandHandler.service();

    // Service getting from serial port
    serviceGetFromSerial();
}

void CommsManager::hdlcFrameRxCBStatic(const uint8_t* pFrame, unsigned frameLen)
{
	_pCommsManager->hdlcFrameRxCB(pFrame, frameLen);
}

static uint32_t msgIdx = 0;

void CommsManager::hdlcFrameRxCB(const uint8_t* pFrame, unsigned frameLen)
{
    CLogger::Get()->Write(MODULE_PREFIX, LogNotice, "hdlc idx %d rx %d", msgIdx++, pFrame[0] * 256 + pFrame[1]);
	CLogger::Get()->Write(MODULE_PREFIX, LogNotice, "hdlc rx");

    // TODO 
	// _pI2cBitBang->clearStats();
}

void CommsManager::hdlcFrameTxCBStatic(const uint8_t* pFrame, unsigned frameLen)
{
	_pCommsManager->hdlcFrameTxCB(pFrame, frameLen);
}

void CommsManager::hdlcFrameTxCB(const uint8_t* pFrame, unsigned frameLen)
{
    if (!_pSerial)
        return;
    _pSerial->Write(pFrame, frameLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommsManager::sendWithJSON(const char* cmdName, const char* cmdJson, uint32_t msgIdx = 0, 
        const uint8_t* pData = NULL, uint32_t dataLen = 0)
{
    _commandHandler.sendWithJSON(cmdName, cmdJson, msgIdx, pData, dataLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function to send to uart from command handler
void CommsManager::serialPutStr(const uint8_t* pBuf, unsigned len)
{
	if (!_pCommsManager->_pSerial)
        return;
    _pCommsManager->_pSerial->Write(pBuf, len);
}

uint32_t CommsManager::serialTxAvailable()
{
    if (!_pCommsManager->_pSerial)
        return 0;
    return _pCommsManager->_pSerial->AvailableForWrite();
}

void CommsManager::serviceGetFromSerial()
{
    if (!_pSerial)
        return;
    // Handle serial communication
	static const int MAX_RX_BUF_LEN = 1000;
	uint8_t serialRxBuf[MAX_RX_BUF_LEN];
	int rxCount = _pSerial->Read(serialRxBuf, MAX_RX_BUF_LEN);
	if (rxCount > 0)
	{
		// 	m_Logger.Write (MODULE_PREFIX, LogNotice, "RxCount %d", rxCount);
		_commandHandler.handleHDLCReceivedChars(serialRxBuf, rxCount);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Received message handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommsManager::handleRxMsg(void* pObject, const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
                char* pRespJson, unsigned maxRespLen)
{
    // CLogger::Get()->Write (MODULE_PREFIX, LogNotice, "RxMsg %s", pCmdJson);

    // Offer to app-serial
    CommsManager* pSourceCommsManager = (CommsManager*)pObject;
    if (pSourceCommsManager && pSourceCommsManager->_pAppSerialIF)
        pSourceCommsManager->_pAppSerialIF->handleRxCmd(pCmdJson, pParams, paramsLen, pRespJson, maxRespLen);

    // // LogWrite(FromBusRaiderApp, LOG_DEBUG, "rxMsg %s", pCmdJson);
    // #define MAX_CMD_NAME_STR 200
    // char cmdName[MAX_CMD_NAME_STR+1];
    // if (!jsonGetValueForKey("cmdName", pCmdJson, cmdName, MAX_CMD_NAME_STR))
    //     return false;

    // pRespJson[0] = 0;

    // if (strcasecmp(cmdName, "getStatus") == 0)
    // {
    //     // Return the status of the Pi app
    //     _pApp->getPiStatus(pRespJson, maxRespLen);
    //     return true;
    // }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Perform update
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommsManager::performUpdate(const uint8_t* pData, unsigned dataLen)
{
	CLogger::Get()->Write(MODULE_PREFIX, LogDebug, "performUpdate dataLen %d firstByte 0x%02x last 0x%02x", 
            dataLen, pData[0], pData[dataLen-1]);

    microsDelay(100000);

	EnableChainBoot(pData, dataLen);
	return true;
}
