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

// #define DEBUG_PROTOCOL_ENDPOINT_DEF
// #define DEBUG_PROTOCOL_ENDPOINT_CREATE_DELETE
// #define DEBUG_PROTOCOL_OUTBOUND_QUEUE
// #define DEBUG_PROTOCOL_INBOUND_QUEUE

// Use a queue
#define PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE

class ProtocolEndpointDef
{
public:

    static const uint32_t INBOUND_BLOCK_MAX_DEFAULT = 1000;
    static const uint32_t INBOUND_QUEUE_MAX_DEFAULT = 5;
    static const uint32_t OUTBOUND_BLOCK_MAX_DEFAULT = 1000;
    static const uint32_t OUTBOUND_QUEUE_MAX_DEFAULT = 5;

    // Pass 0 for blockMax and queueMaxLen default values to be used
    ProtocolEndpointDef(const char* pSourceProtocolName, 
                ProtocolEndpointMsgCB endpointMsgCB, 
                const char* interfaceName, 
                ChannelReadyCBType channelReadyCB,
                uint32_t inboundBlockMax,
                uint32_t inboundQueueMaxLen,
                uint32_t outboundBlockMax, 
                uint32_t outboundQueueMaxLen)
    {
        _channelProtocolName = pSourceProtocolName;
        _endpointMsgCB = endpointMsgCB;
        _interfaceName = interfaceName;
        _channelReadyCB = channelReadyCB;
        _pProtocolHandler = NULL;
        _inboundBlockMax = inboundBlockMax == 0 ? INBOUND_BLOCK_MAX_DEFAULT : inboundBlockMax;
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
        _inboundQueue.setMaxLen(inboundQueueMaxLen == 0 ? INBOUND_QUEUE_MAX_DEFAULT : inboundQueueMaxLen);
#endif
        _outboundBlockMax = outboundBlockMax == 0 ? OUTBOUND_BLOCK_MAX_DEFAULT : outboundBlockMax;
        _outboundQueue.setMaxLen(outboundQueueMaxLen == 0 ? OUTBOUND_QUEUE_MAX_DEFAULT : outboundQueueMaxLen);
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
#ifdef DEBUG_PROTOCOL_ENDPOINT_CREATE_DELETE
        LOG_I("PcolEPtDef", "setProtocolHandler channelID %d", 
                                pProtocolHandler->getChannelID());
#endif
        _pProtocolHandler = pProtocolHandler;
    }

    bool canAcceptData()
    {
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
        return _inboundQueue.canAcceptData();
#else
        return true;
#endif
    }

    void callProtocolHandlerWithRxData(const uint8_t* pMsg, uint32_t msgLen)
    {
        // Debug
#ifdef DEBUG_PROTOCOL_ENDPOINT_DEF
        LOG_I("PcolEPtDef", "callProtocolHandlerWithRxData protocolName %s interfaceName %s len %d handlerPtrOk %s", 
            _channelProtocolName.c_str(), 
            _interfaceName.c_str(), 
            msgLen, 
            (_pProtocolHandler ? "YES" : "NO"));
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

    bool processInboundQueue()
    {
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
        ProtocolRawMsg msg;
        bool msgReady = _inboundQueue.get(msg);
        if (msgReady)
        {
            if (_pProtocolHandler)
               _pProtocolHandler->addRxData(msg.getBuf(), msg.getBufLen());
#ifdef DEBUG_PROTOCOL_INBOUND_QUEUE
            LOG_I("PcolEPtDef", "processInboundQueue msgLen %d", msg.getBufLen());
#endif
        }
        return msgReady;
#endif
        return false;
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
        bool hasGot = _outboundQueue.get(msg);
#ifdef DEBUG_PROTOCOL_OUTBOUND_QUEUE
        if (hasGot)
        {
            LOG_I("PcolEPtDef", "Got from outboundQueue msgLen %d", msg.getBufLen());
        }
#endif
        return hasGot;
    }

    bool checkChannelReady(uint32_t channelID)
    {
        if (_channelReadyCB)
            return _channelReadyCB(channelID);
        return false;
    }

    bool callEndpointMsgCB(ProtocolEndpointMsg& msg)
    {
        if (_endpointMsgCB)
            return _endpointMsgCB(msg);
        return false;
    }

    uint32_t getInboundBlockMax()
    {
        return _inboundBlockMax;
    }

    uint32_t getOutboundBlockMax()
    {
        return _outboundBlockMax;
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

    // Block and queue sizes
    uint16_t _inboundBlockMax;
    uint16_t _outboundBlockMax;

    // Outbound message queue for response messages
    ThreadSafeQueue<ProtocolEndpointMsg> _outboundQueue;

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    // Inbound message queue for raw messages
    ThreadSafeQueue<ProtocolRawMsg> _inboundQueue;
#endif
};
