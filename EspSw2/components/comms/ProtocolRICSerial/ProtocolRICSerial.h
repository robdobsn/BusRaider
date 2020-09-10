/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolRICSerial
// Protocol wrapper implementing RICSerial
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ProtocolBase.h>

class MiniHDLC;

class ProtocolRICSerial : public ProtocolBase
{
public:
    ProtocolRICSerial(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB);
    virtual ~ProtocolRICSerial();
    
    // Create instance
    static ProtocolBase* createInstance(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB)
    {
        return new ProtocolRICSerial(channelID, configJSON, msgTxCB, msgRxCB);
    }

    // // Set message complete callback
    // virtual void setMsgCompleteCB(ProtocolEndpointMsgCB msgCB)
    // {
    //     _msgCB = msgCB;
    // }

    virtual void addRxData(const uint8_t* pData, uint32_t dataLen) override final;
    virtual void encodeTxMsgAndSend(ProtocolEndpointMsg& msg) override final;

private:
    // HDLC
    MiniHDLC* _pHDLC;

    // The following set the "HDLC" special byte values
    static constexpr uint8_t RIC_SERIAL_FRAME_BOUNDARY_OCTET = 0x7E;
    static constexpr uint8_t RIC_SERIAL_CONTROL_ESCAPE_OCTET = 0x7D;

    // Consts
    static const int DEFAULT_RIC_SERIAL_RX_MAX = 5000;
    static const int DEFAULT_RIC_SERIAL_TX_MAX = 5000;

    // Max msg lengths
    uint32_t _maxTxMsgLen;
    uint32_t _maxRxMsgLen;

    // Callbacks
    ProtocolEndpointMsgCB _msgTxCB;
    ProtocolEndpointMsgCB _msgRxCB;

    // Helpers
    void hdlcFrameRxCB(const uint8_t* pFrame, int frameLen);
};
