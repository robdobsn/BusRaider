/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerialMsg
// Some content from Marty V1 rosbridge.cpp/h
//
// Sandy Enoch and Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "ROSTopicManager.h"

static const uint32_t ROSSERIAL_SYNC_FLAG_POS = 0;
static const uint32_t ROSSERIAL_SYNC_FLAG_VAL = 0xff;
static const uint32_t ROSSERIAL_PCOL_FLAG_POS = 1;
static const uint32_t ROSSERIAL_PCOL_FLAG_VAL = 0xfe;
static const uint32_t ROSSERIAL_MSG_LEN_LOW_POS = 2;
static const uint32_t ROSSERIAL_MSG_LEN_HIGH_POS = 3;
static const uint32_t ROSSERIAL_MSG_LEN_CSUM_POS = 4;
static const uint32_t ROSSERIAL_MSG_TOPIC_ID_LOW_POS = 5;
static const uint32_t ROSSERIAL_MSG_TOPIC_ID_HIGH_POS = 6;
static const uint32_t ROSSERIAL_MSG_PAYLOAD_POS = 7;
static const uint32_t ROSSERIAL_MSG_MIN_LENGTH = ROSSERIAL_MSG_PAYLOAD_POS+1;
static const uint32_t ROSSERIAL_MSG_PAYLOAD_CSUM_OFFSET = 7;
static const uint32_t ROSSERIAL_MSG_MIN_LEN = 8;

#define ROS_SPECIALTOPIC_PUBLISHER 0
#define ROS_SPECIALTOPIC_SUBSCRIBER 1
#define ROS_SPECIALTOPIC_SERVICE_SERVER 2
#define ROS_SPECIALTOPIC_SERVICE_CLIENT 4
#define ROS_SPECIALTOPIC_PARAMETER_REQUEST 6
#define ROS_SPECIALTOPIC_LOG 7
#define ROS_SPECIALTOPIC_TIME 10
#define ROS_SPECIALTOPIC_TX_STOP 11

class ROSSerialMsg
{
public:
    // Constructor
    ROSSerialMsg()
    {
        _headerChecksum = 0;
        _topicID = 0;
        _payloadChecksum = 0;
    }

    // Decode and encode
    bool decode(const uint8_t* pBuf, uint32_t len, uint32_t& actualMsgLen);
    void encode(uint16_t topicId, const uint8_t* pPayload, uint32_t payloadLen);
    void writeRawMsgToVector(std::vector<uint8_t>& rawMsg, bool append);
 
    uint32_t getTopicID()
    {
        return _topicID;
    }
    uint32_t getPayloadLen()
    {
        return _payload.size();
    }
    const uint8_t* getPayload()
    {
        return _payload.data();
    }
    uint8_t getPayloadByte(uint32_t pos)
    {
        if (pos < _payload.size())
            return _payload[pos];
        return 0;
    }

private:
    // Vars
    uint8_t _headerChecksum;
    uint16_t _topicID;
    std::vector<uint8_t> _payload;
    uint8_t _payloadChecksum;

    // Helpers
    static uint8_t computeHeaderChecksum(uint16_t payloadLength)
    {
        uint8_t byte1 = (payloadLength >> 8) & 0xFF;
        uint8_t byte2 = payloadLength & 0xFF;
        return (uint8_t)(255 - ((byte1 + byte2) & 0xFF));
    }
    static uint8_t computePayloadChecksum(uint16_t _topicID, const uint8_t *topicPayload,
                             uint32_t payloadLength)
    {
        uint8_t byte1 = (_topicID >> 8) & 0xFF;
        uint8_t byte2 = _topicID & 0xFF;
        uint8_t total = byte1 + byte2;
        for (int i = 0; i < payloadLength; i++)
        {
            total += (uint8_t)topicPayload[i];
        }
        return (uint8_t)(255 - total);
    }
    uint8_t computePayloadChecksum()
    {
        return computePayloadChecksum(_topicID, _payload.data(), _payload.size());
    }
};
