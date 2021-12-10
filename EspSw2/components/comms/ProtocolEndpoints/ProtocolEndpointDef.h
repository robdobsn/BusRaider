/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Protocol Endpoint Definition
// Endpoints for protocol messages
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <Logger.h>
#include <WString.h>
#include <ProtocolBase.h>
#include <ThreadSafeQueue.h>
#include <ProtocolEndpointMsg.h>
#include <ProtocolRawMsg.h>

// Use a queue
#define PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE

class ProtocolEndpointDef
{
    friend class ProtocolEndpointManager;
public:

    static const uint32_t INBOUND_BLOCK_MAX_DEFAULT = 1000;
    static const uint32_t INBOUND_QUEUE_MAX_DEFAULT = 20;
    static const uint32_t OUTBOUND_BLOCK_MAX_DEFAULT = 1000;
    static const uint32_t OUTBOUND_QUEUE_MAX_DEFAULT = 20;

    // xxBlockMax and xxQueueMaxLen parameters can be 0 for defaults to be used
    ProtocolEndpointDef(const char* pSourceProtocolName, 
                ProtocolEndpointMsgCB endpointMsgCB, 
                const char* interfaceName, 
                ChannelReadyCBType channelReadyCB,
                uint32_t inboundBlockMax,
                uint32_t inboundQueueMaxLen,
                uint32_t outboundBlockMax, 
                uint32_t outboundQueueMaxLen);

private:
    String getInterfaceName()
    {
        return _interfaceName;
    }

    String getSourceProtocolName()
    {
        return _channelProtocolName;
    }

    ProtocolBase* getProtocolHandler()
    {
        return _pProtocolHandler;
    }

    // Set protocol handler for channel
    void setProtocolHandler(ProtocolBase* pProtocolHandler);

    // Handle Rx data
    void callProtocolHandlerWithRxData(const uint8_t* pMsg, uint32_t msgLen);

    // Inbound queue
    bool canAcceptInbound();

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    void addToInboundQueue(const uint8_t* pMsg, uint32_t msgLen);
#endif

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    bool getFromInboundQueue(ProtocolRawMsg& msg);
#endif

    uint32_t getInboundBlockMax()
    {
        return _inboundBlockMax;
    }
    bool processInboundQueue();

    // Outbound queue
    void addToOutboundQueue(ProtocolEndpointMsg& msg);
    bool getFromOutboundQueue(ProtocolEndpointMsg& msg);
    uint32_t getOutboundBlockMax()
    {
        return _outboundBlockMax;
    }

    // Call protocol handler with a message
    void callProtocolHandlerWithTxMsg(ProtocolEndpointMsg& msg);

    // Check channel is ready
    bool canAcceptOutbound(uint32_t channelID)
    {
        if (_channelReadyCB)
            return _channelReadyCB(channelID);
        return false;
    }

    // Call endpoint message callback
    bool callEndpointMsgCB(ProtocolEndpointMsg& msg)
    {
        if (_endpointMsgCB)
            return _endpointMsgCB(msg);
        return false;
    }

    // Get info JSON
    String getInfoJSON();

private:
    // Protocol supported
    String _channelProtocolName;

    // Callback
    ProtocolEndpointMsgCB _endpointMsgCB;

    // Name of channel
    String _interfaceName;

    // Protocol handler
    ProtocolBase* _pProtocolHandler;

    // Channel ready callback
    ChannelReadyCBType _channelReadyCB;

    // Inbound queue block max size
    uint16_t _inboundBlockMax;

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    // Inbound message queue for raw messages
    ThreadSafeQueue<ProtocolRawMsg> _inboundQueue;
#endif

    // Inbound queue peak level
    uint16_t _inboundQPeak;

    // Outbound queue block max size
    uint16_t _outboundBlockMax;

    // Outbound message queue for response messages
    ThreadSafeQueue<ProtocolEndpointMsg> _outboundQueue;

    // Outbount queue peak level
    uint16_t _outboundQPeak;
};
