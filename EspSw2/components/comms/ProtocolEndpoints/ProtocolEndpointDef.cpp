/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Protocol Endpoint Definition
// Endpoints for protocol messages
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ProtocolEndpointDef.h>

// Warn
#define WARN_ON_INBOUND_QUEUE_FULL

// Debug
// #define DEBUG_PROTOCOL_ENDPOINT_DEF
// #define DEBUG_PROTOCOL_ENDPOINT_CREATE_DELETE
// #define DEBUG_PROTOCOL_OUTBOUND_QUEUE
// #define DEBUG_PROTOCOL_INBOUND_QUEUE

#if defined(DEBUG_PROTOCOL_ENDPOINT_DEF) || defined(DEBUG_PROTOCOL_ENDPOINT_CREATE_DELETE) || defined(DEBUG_PROTOCOL_OUTBOUND_QUEUE) || defined(DEBUG_PROTOCOL_INBOUND_QUEUE) || defined(WARN_ON_INBOUND_QUEUE_FULL)
static const char* MODULE_PREFIX = "ProtDef";
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
// xxBlockMax and xxQueueMaxLen parameters can be 0 for defaults to be used
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolEndpointDef::ProtocolEndpointDef(const char* pSourceProtocolName, 
            const char* interfaceName, 
            const char* channelName,
            ProtocolEndpointMsgCB endpointMsgCB, 
            ChannelReadyCBType channelReadyCB,
            uint32_t inboundBlockMax,
            uint32_t inboundQueueMaxLen,
            uint32_t outboundBlockMax, 
            uint32_t outboundQueueMaxLen)
            : 
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
            _inboundQueue(inboundQueueMaxLen == 0 ? INBOUND_QUEUE_MAX_DEFAULT : inboundQueueMaxLen),
#endif
            _outboundQueue(outboundQueueMaxLen == 0 ? OUTBOUND_QUEUE_MAX_DEFAULT : outboundQueueMaxLen)
{
    _inboundBlockMax = inboundBlockMax == 0 ? INBOUND_BLOCK_MAX_DEFAULT : inboundBlockMax;
    _outboundBlockMax = outboundBlockMax == 0 ? OUTBOUND_BLOCK_MAX_DEFAULT : outboundBlockMax;
    _channelProtocolName = pSourceProtocolName;
    _endpointMsgCB = endpointMsgCB;
    _interfaceName = interfaceName;
    _channelName = channelName;
    _channelReadyCB = channelReadyCB;
    _pProtocolHandler = NULL;
    _outboundQPeak = 0;
    _inboundQPeak = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setProtocolHandler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointDef::setProtocolHandler(ProtocolBase* pProtocolHandler)
{
#ifdef DEBUG_PROTOCOL_ENDPOINT_CREATE_DELETE
    LOG_I(MODULE_PREFIX, "setProtocolHandler channelID %d", 
                            pProtocolHandler->getChannelID());
#endif
    _pProtocolHandler = pProtocolHandler;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Inbound queue
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolEndpointDef::canAcceptInbound()
{
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    return _inboundQueue.canAcceptData();
#else
    return true;
#endif
}

void ProtocolEndpointDef::callProtocolHandlerWithRxData(const uint8_t* pMsg, uint32_t msgLen)
{
    // Debug
#ifdef DEBUG_PROTOCOL_ENDPOINT_DEF
    LOG_I(MODULE_PREFIX, "callProtocolHandlerWithRxData protocolName %s interfaceName %s channelName %s len %d handlerPtrOk %s", 
        _channelProtocolName.c_str(), 
        _interfaceName.c_str(), 
        _channelName.c_str(),
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
void ProtocolEndpointDef::addToInboundQueue(const uint8_t* pMsg, uint32_t msgLen)
{
    ProtocolRawMsg msg(pMsg, msgLen);
#if defined(DEBUG_PROTOCOL_ENDPOINT_DEF) || defined(WARN_ON_INBOUND_QUEUE_FULL)
    bool addedOk = 
#endif
    _inboundQueue.put(msg);
    if (_inboundQPeak < _inboundQueue.count())
        _inboundQPeak = _inboundQueue.count();
#ifdef DEBUG_PROTOCOL_ENDPOINT_DEF
    LOG_I(MODULE_PREFIX, "addToInboundQueue %slen %d peak %d", addedOk ? "" : "FAILED QUEUE IS FULL ", msgLen, _inboundQPeak);
#endif
#ifdef WARN_ON_INBOUND_QUEUE_FULL
    if (!addedOk)
    {
        LOG_W(MODULE_PREFIX, "addToInboundQuuee QUEUE IS FULL peak %d", _inboundQPeak);
    }
#endif
}
#endif

#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
bool ProtocolEndpointDef::getFromInboundQueue(ProtocolRawMsg& msg)
{
    return _inboundQueue.get(msg);
}
#endif

bool ProtocolEndpointDef::processInboundQueue()
{
#ifdef PROTOCOL_ENDPOINT_USE_INBOUND_QUEUE
    ProtocolRawMsg msg;
    bool msgReady = _inboundQueue.get(msg);
    if (msgReady)
    {
        if (_pProtocolHandler)
            _pProtocolHandler->addRxData(msg.getBuf(), msg.getBufLen());
#ifdef DEBUG_PROTOCOL_INBOUND_QUEUE
        LOG_I(MODULE_PREFIX, "processInboundQueue msgLen %d channelID %d protocolName %s", 
                    msg.getBufLen(), 
                    _pProtocolHandler ? _pProtocolHandler->getChannelID() : -1, 
                    _pProtocolHandler ? _pProtocolHandler->getProtocolName() : "NULL");
#endif
    }
    return msgReady;
#endif
    return false;
}

void ProtocolEndpointDef::callProtocolHandlerWithTxMsg(ProtocolEndpointMsg& msg)
{
    if (_pProtocolHandler)
        _pProtocolHandler->encodeTxMsgAndSend(msg);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Outbound queue
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointDef::addToOutboundQueue(ProtocolEndpointMsg& msg)
{
    _outboundQueue.put(msg);
    if (_outboundQPeak < _outboundQueue.count())
        _outboundQPeak = _outboundQueue.count();

}

bool ProtocolEndpointDef::getFromOutboundQueue(ProtocolEndpointMsg& msg)
{
    bool hasGot = _outboundQueue.get(msg);
#ifdef DEBUG_PROTOCOL_OUTBOUND_QUEUE
    if (hasGot)
    {
        LOG_I(MODULE_PREFIX, "Got from outboundQueue msgLen %d", msg.getBufLen());
    }
#endif
    return hasGot;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get info JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String ProtocolEndpointDef::getInfoJSON()
{
    char jsonInfoStr[200];
    snprintf(jsonInfoStr, sizeof(jsonInfoStr),
            R"("name":"%s","if":"%s","ch":%s,"hdlr":%d,"chanID":%d,"inMax":%d,"inPk":%d,"inBlk":%d,"outMax":%d,"outPk":%d,"outBlk":%d)",
            _channelProtocolName.c_str(), _interfaceName.c_str(), _channelName.c_str(),
            _pProtocolHandler ? 1 : 0,
            _pProtocolHandler ? _pProtocolHandler->getChannelID() : -1,
            _inboundQueue.maxLen(), _inboundQPeak, _inboundBlockMax,
            _outboundQueue.maxLen(), _outboundQPeak, _outboundBlockMax);
    return "{" + String(jsonInfoStr) + "}";
}
