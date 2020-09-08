/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolEndpoint
// Endpoints for protocol messages
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "ProtocolEndpointManager.h"
#include <ProtocolEndpointMsg.h>

static const char* MODULE_PREFIX = "ProtocolEndpointManager";

// #define DEBUG_OUTBOUND_RESPONSE 1
// #define DEBUG_OUTBOUND_PUBLISH 1
// #define DEBUG_INBOUND_MESSAGE 1
// #define DEBUG_SERVICE 1
// #define DEBUG_CHANNEL_ID 1
// #define DEBUG_PROTOCOL_HANDLER 1
// #define DEBUG_FRAME_SEND 1
#define WARN_ON_NO_CHANNEL_MATCH

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolEndpointManager::ProtocolEndpointManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
}

ProtocolEndpointManager::~ProtocolEndpointManager()
{
    // Clean up 
    for (int channelID = 0; channelID < _endpointsVec.size(); channelID++)
    {
        // Check if channel 
        ProtocolBase* pHandler = _endpointsVec[channelID]->getProtocolHandler();
        if (pHandler)
            delete pHandler;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointManager::service()
{
    // Pump protocol queues
    for (int channelID = 0; channelID < _endpointsVec.size(); channelID++)
    {
        // Check if channel can accept an outbound message
        if (_endpointsVec[channelID]->checkChannelReady())
        {
            // Check for outbound message
            ProtocolEndpointMsg msg;
            if (_endpointsVec[channelID]->getFromOutboundQueue(msg))
            {
                // Ensure protocol handler exists
                ensureProtocolHandlerExists(channelID);

                // Debug
#ifdef DEBUG_SERVICE
                LOG_I(MODULE_PREFIX, "service, got msg channelID %d, direction %s msgNum %d, len %d",
                    msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen());
#endif
                // Handle the message
                _endpointsVec[channelID]->callProtocolHandlerWithTxMsg(msg);
            }
        }

        // Process inbound message - possibly multiple messages
        for (int msgIdx = 0; msgIdx < MAX_INBOUND_MSGS_IN_LOOP; msgIdx++)
        {
            _endpointsVec[channelID]->processInboundQueue();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Register a channel
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t ProtocolEndpointManager::registerChannel(const char* protocolName, ProtocolEndpointMsgCB msgCB, 
                    const char* interfaceName, ChannelReadyCBType channelReadyCB)
{
    // Create new command definition and add
    _endpointsList.push_back(ProtocolEndpointDef(protocolName, msgCB, interfaceName, channelReadyCB));
    _endpointsVec.push_back(&_endpointsList.back());

    // Return current callback number
    return _endpointsVec.size()-1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add protocol handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointManager::addProtocol(ProtocolDef& protocolDef)
{
    LOG_D(MODULE_PREFIX, "Adding protocol handler for %s", protocolDef.protocolName.c_str());
    _protocolTypes.push_back(protocolDef);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get channel ID
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t ProtocolEndpointManager::lookupChannelID(const char* interfaceName, const char* protocolName)
{
    // Iterate the endpoints list to find a match
    for (int channelID = 0; channelID < _endpointsVec.size(); channelID++)
    {
#ifdef DEBUG_CHANNEL_ID
        LOG_I(MODULE_PREFIX, "Testing interface %s with %s protocol %s with %s", 
                    _endpointsVec[channelID]->getInterfaceName().c_str(), interfaceName,
                    _endpointsVec[channelID]->getSourceProtocolName().c_str(), protocolName);
#endif
        if (_endpointsVec[channelID]->getInterfaceName().equalsIgnoreCase(interfaceName) && 
            _endpointsVec[channelID]->getSourceProtocolName().equalsIgnoreCase(protocolName))
            return channelID;
    }
#ifdef WARN_ON_NO_CHANNEL_MATCH
    LOG_W(MODULE_PREFIX, "getChannelID noMatch interface %s protocol %s", interfaceName, protocolName);
#endif
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get channel IDs that use a specific protocol
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointManager::getChannelIDsWithProtocol(const char* protocolName, std::vector<uint32_t>& channelIDsWithProtocol)
{
    // Iterate and check
    channelIDsWithProtocol.clear();
    channelIDsWithProtocol.reserve(_endpointsVec.size());
    for (uint32_t channelID = 0; channelID < _endpointsVec.size(); channelID++)
    {
        if (_endpointsVec[channelID]->getSourceProtocolName() == protocolName)
            channelIDsWithProtocol.push_back(channelID);
    }
    channelIDsWithProtocol.shrink_to_fit();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle channel message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointManager::handleInboundMessage(uint32_t channelID, const uint8_t* pMsg, uint32_t msgLen)
{
    // Check the channel
    if (channelID >= _endpointsVec.size())
    {
        LOG_W(MODULE_PREFIX, "handleInboundMessage, channelID channelId %d is INVALID msglen %d", channelID, msgLen);
        return;
    }

    // Debug
#ifdef DEBUG_INBOUND_MESSAGE
    LOG_I(MODULE_PREFIX, "handleInboundMessage, channel Id %d channel name %s, msglen %d", channelID, 
                _endpointsVec[channelID]->getInterfaceName().c_str(), msgLen);
#endif

    // Ensure we have a handler for this msg
    ensureProtocolHandlerExists(channelID);

    // Handle the message
    _endpointsVec[channelID]->callProtocolHandlerWithRxData(pMsg, msgLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle outbound message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointManager::handleOutboundMessage(ProtocolEndpointMsg& msg)
{
    // Get the channel
    uint32_t channelID = msg.getChannelID();
    if (channelID < _endpointsVec.size())
    {
        // Send to one channel
        handleOutboundMessageOnChannel(msg, channelID);
    }
    else if (channelID == MSG_CHANNEL_ID_ALL)
    {
        // Send on all open channels with this protocol
        std::vector<uint32_t> channelIDsWithProtocol;
        getChannelIDsWithProtocol("RICSerial", channelIDsWithProtocol);
        for (uint32_t specificChannelID : channelIDsWithProtocol)
        {
            msg.setChannelID(specificChannelID);
            handleOutboundMessageOnChannel(msg, specificChannelID);
        }
    }
    else
    {
        LOG_W(MODULE_PREFIX, "handleOutboundMessage, channelID INVALID channel Id %u msglen %d", channelID, msg.getBufLen());
    }
}

void ProtocolEndpointManager::handleOutboundMessageOnChannel(ProtocolEndpointMsg& msg, uint32_t channelID)
{
    // Check for message direction, this controls whether a publish message or response
    // response messages are placed in a regular queue
    // publish messages are handled such that only the latest one of any address is sent
    if (msg.getDirection() == MSG_DIRECTION_RESPONSE)
    {
#ifdef DEBUG_OUTBOUND_RESPONSE
        // Debug
        LOG_I(MODULE_PREFIX, "handleOutboundMessage queued channel Id %d channel name %s, msglen %d, direction %s msgNum %d", channelID, 
                    _endpointsVec[channelID]->getInterfaceName().c_str(), msg.getBufLen(), msg.getDirectionAsString(msg.getDirection()),
                    msg.getMsgNumber());
#endif
        if (_endpointsVec[channelID])
            _endpointsVec[channelID]->addToOutboundQueue(msg);
    }
    else
    {
            // TODO - maybe on callback thread here so make sure this is ok!!!!
            // TODO - probably have a single-element buffer for each publish type???
            //      - then service it in the service loop

            // Ensure protocol handler exists
            ensureProtocolHandlerExists(channelID);

#ifdef DEBUG_OUTBOUND_PUBLISH
            // Debug
            LOG_I(MODULE_PREFIX, "handleOutboundMessage msg channelID %d, direction %s msgNum %d, len %d",
                msg.getChannelID(), msg.getDirectionAsString(msg.getDirection()), msg.getMsgNumber(), msg.getBufLen());
#endif

            // Check if channel can accept an outbound message and send if so
            if (_endpointsVec[channelID] && _endpointsVec[channelID]->checkChannelReady())
                _endpointsVec[channelID]->callProtocolHandlerWithTxMsg(msg);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Ensure protocol handler exists - lazy creation of handler
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolEndpointManager::ensureProtocolHandlerExists(uint32_t channelID)
{
    // Check if we already have a handler for this msg
    if ((!_endpointsVec[channelID]) || (_endpointsVec[channelID]->getProtocolHandler() != NULL))
        return;


    // If not we need to get one
    String channelProtocol = _endpointsVec[channelID]->getSourceProtocolName();

    // Debug
#ifdef DEBUG_PROTOCOL_HANDLER
    LOG_I(MODULE_PREFIX, "No handler specified so creating one for channelID %d protocol %s",
            channelID, channelProtocol.c_str());
#endif

    // Find the protocol in the list
    for (ProtocolDef& protocolDef : _protocolTypes)
    {
        if (protocolDef.protocolName == channelProtocol)
        {
            // Debug
#ifdef DEBUG_PROTOCOL_HANDLER
            LOG_I(MODULE_PREFIX, "ensureProtocolHandlerExists channelID %d protocol %s params %s",
                    channelID, channelProtocol.c_str(), protocolDef.paramsJSON.c_str());
#endif

            // Create a protocol object
            ProtocolBase* pProtocolHandler = protocolDef.createFn(channelID, protocolDef.paramsJSON.c_str(), 
                        std::bind(&ProtocolEndpointManager::frameSendCB, this, std::placeholders::_1), 
                        protocolDef.frameRxCB);
            _endpointsVec[channelID]->setProtocolHandler(pProtocolHandler);
            return;
        }
    }

    // Debug
    LOG_W(MODULE_PREFIX, "No suitable handler found for protocol %s map entries %d", channelProtocol.c_str(), _protocolTypes.size());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ProtocolEndpointManager::frameSendCB(ProtocolEndpointMsg& msg)
{
#ifdef DEBUG_FRAME_SEND
    LOG_I(MODULE_PREFIX, "frameSendCB (response/publish) frame type %s len %d", msg.getProtocolAsString(msg.getProtocol()), msg.getBufLen());
#endif

    uint32_t channelID = msg.getChannelID();
    if (channelID >= _endpointsVec.size())
    {
        LOG_W(MODULE_PREFIX, "frameSendCB, channelID INVALID channel Id %d msglen %d", channelID, msg.getBufLen());
        return false;
    }

#ifdef DEBUG_FRAME_SEND
    // Debug
    LOG_I(MODULE_PREFIX, "frameSendCB, channel Id %d channel name %s, msglen %d", channelID, 
                _endpointsVec[channelID]->getInterfaceName().c_str(), msg.getBufLen());
#endif

    // Send back to the channel
    return _endpointsVec[channelID]->callEndpointMsgCB(msg);
}

