/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRICFrame
// Protocol wrapper implementing RICFrame
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ProtocolRICFrame.h"
#include <ProtocolEndpointMsg.h>
#include <ConfigBase.h>

// Logging
static const char* MODULE_PREFIX = "PcolRICFrm";

// Debug
// #define DEBUG_PROTOCOL_RIC_FRAME

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolRICFrame::ProtocolRICFrame(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB) :
    ProtocolBase(channelID), _msgTxCB(msgTxCB), _msgRxCB(msgRxCB)
{
    // Extract configuration
    ConfigBase config(configJSON);
    uint32_t maxRxMsgLen = config.getLong("MaxRxMsgLen", DEFAULT_RIC_FRAME_RX_MAX);
    uint32_t maxTxMsgLen = config.getLong("MaxTxMsgLen", DEFAULT_RIC_FRAME_TX_MAX);
    // Debug
    LOG_I(MODULE_PREFIX, "constructed maxRxMsgLen %d maxTxMsgLen %d", maxRxMsgLen, maxTxMsgLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolRICFrame::~ProtocolRICFrame()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// addRxData
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolRICFrame::addRxData(const uint8_t* pData, uint32_t dataLen)
{
    // Debug
    // LOG_V(MODULE_PREFIX, "addRxData len %d", dataLen);

    // Check callback is valid
    if (!_msgRxCB)
        return;

    // Check validity of frame length
    if (dataLen < 2)
        return;

    // Extract message type
    uint32_t msgNumber = pData[0];
    uint32_t msgProtocolCode = pData[1] & 0x3f;
    uint32_t msgDirectionCode = pData[1] >> 6;

    // Debug
#ifdef DEBUG_PROTOCOL_RIC_FRAME
    LOG_I(MODULE_PREFIX, "addRxData len %d msgNum %d protocolCode %d dirn %d", 
                    dataLen, msgNumber, msgProtocolCode, msgDirectionCode);
#endif

    // Convert to ProtocolEndpointMsg
    ProtocolEndpointMsg endpointMsg;
    endpointMsg.setFromBuffer(_channelID, (ProtocolMsgProtocol)msgProtocolCode, msgNumber, (ProtocolMsgDirection)msgDirectionCode, pData+2, dataLen-2);
    _msgRxCB(endpointMsg);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// addRxData
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolRICFrame::encodeTxMsgAndSend(ProtocolEndpointMsg& msg)
{
    // Valid
    if (!_msgTxCB)
    {
        #ifdef DEBUG_PROTOCOL_RIC_FRAME
            // Debug
            LOG_I(MODULE_PREFIX, "encodeTxMsgAndSend NO TX CB");
        #endif
        return;
    }

    // Create the message
    std::vector<uint8_t> ricFrameMsg;
    ricFrameMsg.reserve(msg.getBufLen()+2);
    ricFrameMsg.push_back(msg.getMsgNumber());
    uint8_t protocolDirnByte = ((msg.getDirection() & 0x03) << 6) + (msg.getProtocol() & 0x3f);
    ricFrameMsg.push_back(protocolDirnByte);
    ricFrameMsg.insert(ricFrameMsg.end(), msg.getCmdVector().begin(), msg.getCmdVector().end());
    msg.setFromBuffer(ricFrameMsg.data(), ricFrameMsg.size());

#ifdef DEBUG_PROTOCOL_RIC_FRAME
    // Debug
    LOG_I(MODULE_PREFIX, "encodeTxMsgAndSend, encoded len %d", msg.getBufLen());
#endif

    // Send
    _msgTxCB(msg);
}
