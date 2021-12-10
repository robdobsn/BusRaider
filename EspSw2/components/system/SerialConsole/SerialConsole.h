/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Serial Console
// Provides serial terminal access to REST API and diagnostics
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <SysModBase.h>
#include <ProtocolOverAscii.h>

class RestAPIEndpointManager;
class ProtocolEndpointManager;
class ProtocolEndpointMsg;
class ConfigBase;

class SerialConsole : public SysModBase
{
public:
    SerialConsole(const char* pModuleName, ConfigBase& defaultConfig, ConfigBase* pGlobalConfig, ConfigBase* pMutableConfig);

    // Get
    int getChar();

    // Put
    void putStr(const char* pStr);
    void putStr(const String& str);

    // Get the state of the reception of Commands 
    // Maybe:
    //   idle = 'i' = no command entry in progress,
    //   newChar = XOFF = new command char received since last call - pause transmission
    //   waiting = 'w' = command incomplete but no new char since last call
    //   complete = XON = command completed - resume transmission
    typedef char CommandRxState;
    CommandRxState getXonXoff();

protected:
    // Setup
    virtual void setup() override final;

    // Service - called frequently
    virtual void service() override final;

    // Add endpoints
    virtual void addRestAPIEndpoints(RestAPIEndpointManager& endpointManager) override final;
    
    // Add protocol endpoints
    virtual void addProtocolEndpoints(ProtocolEndpointManager &endpointManager) override final;

    // Handle JSON command
    virtual void receiveCmdJSON(const char* cmdJSON) override final;

public:
    // XON/XOFF hadnling
    static constexpr char ASCII_XOFF = 0x13;
    static constexpr char ASCII_XON = 0x11;

    // State
    static constexpr CommandRxState CommandRx_idle = 'i';
    static constexpr CommandRxState CommandRx_newChar = ASCII_XOFF;
    static constexpr CommandRxState CommandRx_waiting = 'w';
    static constexpr CommandRxState CommandRx_complete = ASCII_XON;

private:
    // isEnabled and isInitialised
    bool _isEnabled;
    bool _isInitialised;

    // CRLF string on tx line end
    bool _crlfOnTx;

    // Serial details
    int _uartNum;
    int _baudRate;

    // Buffer sizes
    uint32_t _rxBufferSize;
    uint32_t _txBufferSize;

    // Procotol
    String _protocol;

    // Line being entered
    String _curLine;

    // Line limits
    static const int MAX_REGULAR_LINE_LEN = 100;
    static const int ABS_MAX_LINE_LEN = 1000;

    // Prev char entered (for line end checks)
    int _prevChar;

    // Cur state (XON/XOFF etc)
    CommandRxState _cmdRxState;

    // EndpointID used to identify this message channel to the ProtocolEndpointManager object
    uint32_t _protocolEndpointID;

    // Protocol message handling
    static const uint32_t INBOUND_BLOCK_MAX_DEFAULT = 5000;
    static const uint32_t INBOUND_QUEUE_MAX_DEFAULT = 20;
    static const uint32_t OUTBOUND_BLOCK_MAX_DEFAULT = 5000;
    static const uint32_t OUTBOUND_QUEUE_MAX_DEFAULT = 5;    

    // ProtocolOverAscii to handle comms through this serial port with MSB set
    static const uint32_t PROTOCOL_OVER_ASCII_MSG_MAX_LEN = 1000;
    ProtocolOverAscii _protocolOverAscii;

    // Helpers
    void showEndpoints();
    bool sendMsg(ProtocolEndpointMsg& msg);
    bool readyToSend(uint32_t channelID);    
};
