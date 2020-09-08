/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PiCoProcessor
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>
#include "MiniHDLC.h"
#include <list>

class ProtocolEndpointMsg;

class PiCoProcessor : public SysModBase
{
public:
    // Constructor/destructor
    PiCoProcessor(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~PiCoProcessor();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager &endpointManager) override final;

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager &endpointManager) override final;

    // Get HDLC stats
    MiniHDLCStats* getHDLCStats()
    {
        if (!_pHDLC)
            return NULL;
        return _pHDLC->getStats();
    }

private:
    // Vars
    bool _isEnabled;

    // Serial details
    int _uartNum;
    int _baudRate;
    int _txPin;
    int _rxPin;
    uint32_t _rxBufSize;
    uint32_t _txBufSize;

    // Flag indicating begun
    bool _isInitialised;
    String _protocol;

    // EndpointID used to identify this message channel to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;

    // Pi status
    String _cachedPiStatusJSON;
    uint32_t _cachedPiStatusRequestMs;
    static const int TIME_BETWEEN_PI_STATUS_REQS_MS = 10000;

    // HDLC processor
    MiniHDLC* _pHDLC;
    uint32_t _hdlcMaxLen;

    // Helpers
    void applySetup();
    void sendToPi(const uint8_t *pFrame, int frameLen);
    void hdlcFrameTxCB(const uint8_t* pFrame, int frameLen);
    void hdlcFrameRxCB(const uint8_t* pFrame, int frameLen);
};
