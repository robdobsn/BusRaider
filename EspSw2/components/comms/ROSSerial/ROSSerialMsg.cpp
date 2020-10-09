/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerialMsg
// Some content from Marty V1 rosbridge.cpp/h
//
// Sandy Enoch and Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Logger.h"
#include "ROSSerialMsg.h"

// Logging
static const char* MODULE_PREFIX = "ROSSerialMsg";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Decode message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ROSSerialMsg::decode(const uint8_t* pBuf, uint32_t len, uint32_t& actualMsgLen)
{
    _payload.clear();
    // Check not too short
    // See http://wiki.ros.org/rosserial/Overview/Protocol
    if (len < ROSSERIAL_MSG_MIN_LENGTH)
        return false;

    // Length
    uint32_t payloadLength = pBuf[ROSSERIAL_MSG_LEN_LOW_POS] + ((uint32_t)(pBuf[ROSSERIAL_MSG_LEN_HIGH_POS]) * 256);
    _headerChecksum = pBuf[ROSSERIAL_MSG_LEN_CSUM_POS];
    _topicID = pBuf[ROSSERIAL_MSG_TOPIC_ID_LOW_POS] + ((uint32_t)(pBuf[ROSSERIAL_MSG_TOPIC_ID_HIGH_POS]) * 256);

    // Check length is ok
    if (len < payloadLength + ROSSERIAL_MSG_MIN_LENGTH)
        return false;
    
    // Set payload
    const uint8_t* pPayload = pBuf + ROSSERIAL_MSG_PAYLOAD_POS;
    _payload.assign(pPayload, pPayload + payloadLength);

    // Final checksum
    _payloadChecksum = pBuf[payloadLength + ROSSERIAL_MSG_PAYLOAD_CSUM_OFFSET];
    actualMsgLen = payloadLength + ROSSERIAL_MSG_PAYLOAD_CSUM_OFFSET + 1;
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Form message
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ROSSerialMsg::encode(uint16_t topicId, const uint8_t* pPayload, uint32_t payloadLen)
{
    _topicID = topicId;
    _payload.assign(pPayload, pPayload + payloadLen);

    // Compute checksums
    _headerChecksum = computeHeaderChecksum(_payload.size());
    _payloadChecksum = computePayloadChecksum();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set from topic
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t ROSSerialMsg::encode(uint32_t specialTopicID, const ROSTopicInfo& info)
{
    // Special topic ID used to say whether this is pub or sub
    _topicID = specialTopicID;

    // Store the payload
    // topicID + 06 00 00 00 +
    // topic_name + 0d 00 00 00 +
    // message_type + 00 00 00 +
    // md5 string + buffer_size
    uint32_t payloadSize = sizeof(info.topic_id) + 4 +
                            strlen(info.topic_name) + 4 +
                            strlen(info.message_type) + 4 +
                            strlen(info.md5sum) + sizeof(info.buffer_size);

    uint16_t bufPos = 0;
    _payload.resize(payloadSize);
    _payload[bufPos++] = (uint8_t)(info.topic_id & 0x00FF);        // low byte
    _payload[bufPos++] = (uint8_t)((info.topic_id & 0xFF00) >> 8); // high byte
    bufPos = uint32ToPayload((uint32_t)strlen(info.topic_name), bufPos);
    bufPos = strToPayload(info.topic_name, bufPos);
    bufPos = uint32ToPayload((uint32_t)strlen(info.message_type), bufPos);
    bufPos = strToPayload(info.message_type, bufPos);
    bufPos = uint32ToPayload((uint32_t)strlen(info.md5sum), bufPos);
    bufPos = strToPayload(info.md5sum, bufPos);
    bufPos = uint32ToPayload(info.buffer_size, bufPos);

    // Compute checksums
    _headerChecksum = computeHeaderChecksum(_payload.size());
    _payloadChecksum = computePayloadChecksum();

    LOG_D(MODULE_PREFIX, "encode len %d bufPos %d", payloadSize, bufPos);

    return bufPos;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generate message to a vector
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ROSSerialMsg::writeRawMsgToVector(std::vector<uint8_t>& rawMsg, bool append)
{
    uint32_t bufPos = (append ? rawMsg.size() : 0);
    uint32_t rawMsgLen = bufPos + _payload.size() + 8;
    rawMsg.resize(rawMsgLen);
    rawMsg[bufPos++] = 0xff;
    rawMsg[bufPos++] = 0xfe;
    rawMsg[bufPos++] = _payload.size() & 0xff;
    rawMsg[bufPos++] = (_payload.size() >> 8) & 0xff;
    rawMsg[bufPos++] = _headerChecksum;
    rawMsg[bufPos++] = _topicID & 0xff;
    rawMsg[bufPos++] = (_topicID >> 8) & 0xff;
    memcpy(rawMsg.data() + bufPos, _payload.data(), _payload.size());
    rawMsg[_payload.size() + bufPos] = _payloadChecksum;
}
