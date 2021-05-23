/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolEndpointManager
// Endpoints for protocol messages
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <WString.h>
#include <vector>
#include <ProtocolBase.h>
#include "ProtocolDef.h"
#include "ProtocolEndpointDef.h"
#include "SysModBase.h"

// Collection of endpoints
class ProtocolEndpointManager : public SysModBase
{
public:
    ProtocolEndpointManager(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig);
    virtual ~ProtocolEndpointManager();

    // Register as an external message channel
    // Returns an ID used to identify this channel
    uint32_t registerChannel(const char* protocolName, ProtocolEndpointMsgCB msgCB, 
                const char* channelName, ChannelReadyCBType channelReadyCB,
                uint32_t inboundBlockMax, uint32_t inboundQueueMaxLen,
                uint32_t outboundBlockMax, uint32_t outboundQueueMaxLen);

    // Register as an internal message sink
    uint32_t registerSink(ProtocolEndpointMsgCB msgCB);

    // Add protocol handler
    void addProtocol(ProtocolDef& protocolDef);

    // Lookup channel ID
    int32_t lookupChannelID(const char* interfaceName, const char* protocolName);

    // Check if we can accept inbound message
    bool canAcceptInbound(uint32_t channelID);
    
    // Handle channel message
    void handleInboundMessage(uint32_t channelID, const uint8_t* pMsg, uint32_t msgLen);

    // Handle outbound message
    void handleOutboundMessage(ProtocolEndpointMsg& msg);

    // Get the optimal comms block size
    uint32_t getInboundBlockMax(uint32_t channelID, uint32_t defaultSize);

    // Undefined endpointID
    static const uint32_t UNDEFINED_ID = 0xffff;

protected:
    // Service - called frequently
    virtual void service() override final;

private:
    // List and vector of endpoints
    // Both are used to store essentially the same information
    // The list stores the actual objects and won't need to copy them when additional
    // objects are added (if a vector were used for this the would be a chance of a copy
    // process occurring when the vector hit a threshold and the objects stored don't
    // handle this properly)
    // The vector stores pointers to the objects and allows direct indexing
    std::list<ProtocolEndpointDef> _endpointsList;
    std::vector<ProtocolEndpointDef*> _endpointsVec;

    // List of protocol translations
    std::list<ProtocolDef> _protocolTypes;

    // Callbacks
    bool frameSendCB(ProtocolEndpointMsg& msg);

    // Helpers
    void ensureProtocolHandlerExists(uint32_t channelID);
    void handleOutboundMessageOnChannel(ProtocolEndpointMsg& msg, uint32_t channelID);
    void getChannelIDs(std::vector<uint32_t>& channelIDs);

    // Consts
    static const int MAX_INBOUND_MSGS_IN_LOOP = 10;
};
