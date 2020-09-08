/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRICFrame
// Protocol wrapper implementing RICFrame
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ProtocolBase.h>

class MiniHDLC;

class ProtocolRICFrame : public ProtocolBase
{
public:
    ProtocolRICFrame(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB);
    virtual ~ProtocolRICFrame();
    
    // Create instance
    static ProtocolBase* createInstance(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB)
    {
        return new ProtocolRICFrame(channelID, configJSON, msgTxCB, msgRxCB);
    }

    // // Set message complete callback
    // virtual void setMsgCompleteCB(ProtocolEndpointMsgCB msgCB)
    // {
    //     _msgCB = msgCB;
    // }

    virtual void addRxData(const uint8_t* pData, uint32_t dataLen) override final;
    virtual void encodeTxMsgAndSend(ProtocolEndpointMsg& msg) override final;

private:
    // Consts
    static const int DEFAULT_RIC_FRAME_RX_MAX = 1000;
    static const int DEFAULT_RIC_FRAME_TX_MAX = 1000;

    // Callbacks
    ProtocolEndpointMsgCB _msgTxCB;
    ProtocolEndpointMsgCB _msgRxCB;
};
