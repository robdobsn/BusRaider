/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CommandSerial
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ConfigBase.h>
#include <RestAPIEndpointManager.h>
#include <SysModBase.h>
#include <list>

class ProtocolEndpointMsg;

class CommandSerial : public SysModBase
{
public:
    // Constructor/destructor
    CommandSerial(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~CommandSerial();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager &endpointManager) override final;

    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager &endpointManager) override final;

private:
    // Helpers
    void applySetup();
    void begin();
    void end();
    bool sendMsg(ProtocolEndpointMsg& msg);
    bool readyToSend();

    // Vars
    bool _isEnabled;

    // Serial details
    int _uartNum;
    int _baudRate;
    int _txPin;
    int _rxPin;
    uint32_t _rxBufSize;
    uint32_t _txBufSize;
    uint32_t _hdlcRxMaxLen;

    // Flag indicating begun
    bool _isInitialised;
    String _protocol;

    // EndpointID used to identify this message channel to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;
};
