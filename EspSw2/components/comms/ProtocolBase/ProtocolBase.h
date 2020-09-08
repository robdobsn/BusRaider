/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolBase
// Base class for protocol translations
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <functional>

class ProtocolEndpointMsg;
class ProtocolBase;

// Put byte callback function type
typedef std::function<void(uint8_t ch)> ProtocolBasePutByteCBType;
// Received frame callback function type
typedef std::function<void(const uint8_t *framebuffer, int framelength)> ProtocolBaseFrameCBType;
// Endpoint message received
typedef std::function<bool(ProtocolEndpointMsg& msg)> ProtocolEndpointMsgCB;
// Create protocol instance
typedef std::function<ProtocolBase* (uint32_t channelID, const char* pConfigJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB)> ProtocolCreateFnType;
// Channel ready function type
typedef std::function<bool()> ChannelReadyCBType;

class ProtocolBase
{
public:
    ProtocolBase(uint32_t channelID)
            : _channelID(channelID)
    {
    }
    virtual ~ProtocolBase()
    {
    }
    virtual void addRxData(const uint8_t* pData, uint32_t dataLen) = 0;
    virtual void encodeTxMsgAndSend(ProtocolEndpointMsg& msg) = 0;

protected:
    // The channelID for messages handled by this protocol handler
    uint32_t _channelID;
};
