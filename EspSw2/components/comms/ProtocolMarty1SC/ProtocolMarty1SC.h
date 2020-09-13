/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolMarty1SC
// Protocol wrapper implementing Marty1ShortCodes
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Logger.h>
#include <ProtocolBase.h>
#include "ProtocolMarty1SCDecoder.h"

class ProtocolMarty1SC : public ProtocolBase
{
public:
    ProtocolMarty1SC(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB);
    virtual ~ProtocolMarty1SC();

    // Create instance
    static ProtocolBase* createInstance(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB)
    {
        return new ProtocolMarty1SC(channelID, configJSON, msgTxCB, msgRxCB);
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
    static const int DEFAULT_MARTY1SC_RX_MAX = 100;
    static const int DEFAULT_MARTY1SC_TX_MAX = 100;

    // Msg lengths
    uint32_t _maxRxMsgLen;
    uint32_t _maxTxMsgLen;

    // Callbacks
    ProtocolEndpointMsgCB _msgTxCB;
    ProtocolEndpointMsgCB _msgRxCB;

    // Decoder
    ProtocolMarty1SCDecoder* _pCmdDecoder;

    // Helpers
    void hdlcFrameRxCB(const uint8_t* pFrame, int frameLen);
    void stdMsgCallback(int id, size_t length);
    void rosMsgCallback(int id, size_t length);
    void sensorMsgCallback(int id, uint8_t sensor, uint8_t sensor_id);
    static uint8_t checksum1(uint16_t payload_size)
    {
        uint8_t byte1 = (payload_size >> 8) & 0xFF;
        uint8_t byte2 = payload_size & 0xFF;
        return (uint8_t) (255 - ((byte1 + byte2) & 0xFF));
    }
    uint8_t checksum2(uint16_t topic_id, uint8_t* payload_ptr, size_t payload_size) 
    {
        uint8_t byte1 = (topic_id >> 8) & 0xFF;
        uint8_t byte2 = topic_id & 0xFF;
        uint8_t total = byte1 + byte2;
        uint8_t* payload_end = payload_ptr + payload_size;
        while (payload_ptr < payload_end) {
            total += *payload_ptr++;
        }
        return (uint8_t) (255 - total);
    }

};
