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
    // xxBlockMax and xxQueueMaxLen parameters can be 0 for defaults to be used
    // Returns an ID used to identify this channel
    uint32_t registerChannel(const char* protocolName, ProtocolEndpointMsgCB msgCB, 
                const char* channelName, ChannelReadyCBType channelReadyCB,
                uint32_t inboundBlockMax = 0, uint32_t inboundQueueMaxLen = 0,
                uint32_t outboundBlockMax = 0, uint32_t outboundQueueMaxLen = 0);

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

    // Check if we can accept outbound message
    bool canAcceptOutbound(uint32_t channelID);
    
    // Handle outbound message
    void handleOutboundMessage(ProtocolEndpointMsg& msg);

    // Get the optimal comms block size
    uint32_t getInboundBlockMax(uint32_t channelID, uint32_t defaultSize);

    // Undefined endpointID
    static const uint32_t UNDEFINED_ID = 0xffff;

    // Get info
    String getInfoJSON();

protected:
    // Service - called frequently
    virtual void service() override final;

private:
    // vector of endpoint pointers - pointer must be deleted and vector
    // element set to nullptr is the endpoint is deleted
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
    static const int MAX_INBOUND_MSGS_IN_LOOP = 1;
};