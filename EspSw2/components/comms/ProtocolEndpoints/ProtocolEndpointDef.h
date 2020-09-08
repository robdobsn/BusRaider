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
#include <RingBufferRTOS.h>
#include <ProtocolEndpointMsg.h>
#include <ProtocolRawMsg.h>

//#define DEBUG_PROTOCOL_ENDPOINT_DEF 1
#define PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE 1

class ProtocolEndpointDef
{
public:

    ProtocolEndpointDef(const char* pSourceProtocolName, ProtocolEndpointMsgCB endpointMsgCB, 
                const char* interfaceName, ChannelReadyCBType channelReadyCB)
    {
        _channelProtocolName = pSourceProtocolName;
        _endpointMsgCB = endpointMsgCB;
        _interfaceName = interfaceName;
        _channelReadyCB = channelReadyCB;
        _pProtocolHandler = NULL;
    }

public:
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

    void setProtocolHandler(ProtocolBase* pProtocolHandler)
    {
        _pProtocolHandler = pProtocolHandler;
    }

    void callProtocolHandlerWithRxData(const uint8_t* pMsg, uint32_t msgLen)
    {
        // Debug
#ifdef DEBUG_PROTOCOL_ENDPOINT_DEF
        LOG_I("PcolEPtDef", "callProtocolHandlerWithRxData protocolName %s interfaceName %s len %d handlerPtrOk %s", 
                                        _channelProtocolName.c_str(), _interfaceName.c_str(), msgLen, (_pProtocolHandler ? "YES" : "NO"));
#endif
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
        addToInboundQueue(pMsg, msgLen);
#else
    if (_pProtocolHandler)
        _pProtocolHandler->addRxData(pMsg, msgLen);
#endif
    }

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    void addToInboundQueue(const uint8_t* pMsg, uint32_t msgLen)
    {
        ProtocolRawMsg msg(pMsg, msgLen);
        _inboundQueue.put(msg);
#ifdef DEBUG_PROTOCOL_ENDPOINT_DEF
        LOG_I("ProtocolEndpointDef", "Appending msg to inbound queue len %d", msgLen);
#endif
    }
#endif

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    bool getFromInboundQueue(ProtocolRawMsg& msg)
    {
        return _inboundQueue.get(msg);
    }
#endif

    void processInboundQueue()
    {
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
        ProtocolRawMsg msg;
        if (_inboundQueue.get(msg))
        {
            if (_pProtocolHandler)
               _pProtocolHandler->addRxData(msg.getBuf(), msg.getBufLen());
        }
#endif
    }

    void callProtocolHandlerWithTxMsg(ProtocolEndpointMsg& msg)
    {
        if (_pProtocolHandler)
            _pProtocolHandler->encodeTxMsgAndSend(msg);
    }

    void addToOutboundQueue(ProtocolEndpointMsg& msg)
    {
        _outboundQueue.put(msg);
    }

    bool getFromOutboundQueue(ProtocolEndpointMsg& msg)
    {
        return _outboundQueue.get(msg);
    }

    bool checkChannelReady()
    {
        if (_channelReadyCB)
            return _channelReadyCB();
        return false;
    }

    bool callEndpointMsgCB(ProtocolEndpointMsg& msg)
    {
        if (_endpointMsgCB)
            return _endpointMsgCB(msg);
        return false;
    }
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

    // Outbound message queue for response messages
    static const int OUTBOUND_MSG_QUEUE_SIZE = 20;
    RingBufferRTOS<ProtocolEndpointMsg, OUTBOUND_MSG_QUEUE_SIZE> _outboundQueue;

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    // Inbound message queue for raw messages
    static const int INBOUND_MSG_QUEUE_SIZE = 20;
    RingBufferRTOS<ProtocolRawMsg, INBOUND_MSG_QUEUE_SIZE> _inboundQueue;
#endif
};
