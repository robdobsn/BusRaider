/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MQTT Protocol
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MQTTProtocol.h"
#include "Utils.h"

// Log prefix
static const char *MODULE_PREFIX = "MQTTProto";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MQTT Protocol Defs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MQTTCONNECT     (1 << 4)  // Client request to connect to Server
#define MQTTCONNACK     (2 << 4)  // Connect acknowledgment
#define MQTTPUBLISH     (3 << 4)  // Publish message
#define MQTTPUBACK      (4 << 4)  // Publish acknowledgment
#define MQTTPUBREC      (5 << 4)  // Publish received (assured delivery part 1)
#define MQTTPUBREL      (6 << 4)  // Publish release (assured delivery part 2)
#define MQTTPUBCOMP     (7 << 4)  // Publish complete (assured delivery part 3)
#define MQTTSUBSCRIBE   (8 << 4)  // Client subscribe request
#define MQTTSUBACK      (9 << 4)  // Subscribe acknowledgment
#define MQTTUNSUBSCRIBE (10 << 4) // Unsubscribe request
#define MQTTUNSUBACK    (11 << 4) // Unsubscribe acknowledgment
#define MQTTPINGREQ     (12 << 4) // PING request
#define MQTTPINGRESP    (13 << 4) // PING response
#define MQTTDISCONNECT  (14 << 4) // Client is disconnecting
#define MQTTReserved    (15 << 4) // Reserved

#define MQTTQOS0        (0 << 1)
#define MQTTQOS1        (1 << 1)
#define MQTTQOS2        (2 << 1)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MQTTProtocol::MQTTProtocol(MQTTPutFnType putFn, MQTTFrameFnType frameRxFn)
{
    _rxBufMaxLen = MQTT_DEFAULT_FRAME_MAX_LEN;
    _putFn = putFn;
    _frameRxFn = frameRxFn;
    _packetID = 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Destructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MQTTProtocol::~MQTTProtocol()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Encode connect packet
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MQTTProtocol::encodeMQTTConnect(std::vector<uint8_t>& msgBuf, uint32_t keepAliveSecs, const char* clientID)
{
    // Connect using no Will, username or password
    static const uint32_t MQTT_PROTOCOL_VERSION = 0x04;
    msgBuf = {MQTTCONNECT,0,0x00,0x04,'M','Q','T','T',MQTT_PROTOCOL_VERSION};

    // Flags
    uint8_t connFlags = 0x02;
    msgBuf.push_back(connFlags);

    // Keep-alive
    msgBuf.push_back(keepAliveSecs / 256);
    msgBuf.push_back(keepAliveSecs % 256);

    // Client ID
    uint32_t bytesAdded = bufferAppendString(msgBuf, clientID);

    // Fixup the length
    msgBuf[1] = 10 + bytesAdded;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Encode ping-req packet
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MQTTProtocol::encodeMQTTPingReq(std::vector<uint8_t>& msgBuf)
{
    // Form message
    msgBuf = {MQTTPINGREQ,0};
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Encode subscription
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MQTTProtocol::encodeMQTTSubscribe(std::vector<uint8_t>& msgBuf, const char* topicFilter, uint32_t qos)
{
    // Form message
    msgBuf = {MQTTSUBSCRIBE + 0x02,0,(uint8_t)(_packetID/256),(uint8_t)(_packetID%256)};
    _packetID++;
    if (_packetID == 0)
        _packetID++;

    // Client ID
    uint32_t bytesAdded = bufferAppendString(msgBuf, topicFilter);

    // QoS
    msgBuf.push_back(qos);

    // Fixup the length
    msgBuf[1] = 2 + bytesAdded + 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Encode publish
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void MQTTProtocol::encodeMQTTPublish(std::vector<uint8_t>& msgBuf, const char* topicStr, const char* msgStr)
{
    // Form message
    msgBuf = {MQTTPUBLISH + 0x02,0};

    // Topic name
    uint32_t bytesAdded = bufferAppendString(msgBuf, topicStr);

    // Append packet ID
    msgBuf.push_back(_packetID/256);
    msgBuf.push_back(_packetID%256);
    _packetID++;
    if (_packetID == 0)
        _packetID++;

    // Payload string
    uint32_t payloadLen = strlen(msgStr);
    msgBuf.insert(msgBuf.end(), (uint8_t*)msgStr, (uint8_t*)msgStr + payloadLen);
    bytesAdded += payloadLen;

    // Fixup the length
    msgBuf[1] = 2 + bytesAdded;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Append string to buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t MQTTProtocol::bufferAppendString(std::vector<uint8_t>& buf, const char* pStr)
{
    // String beforeStr;
    // Utils::getHexStrFromBytes(buf.data(), buf.size(), beforeStr);

    // Set length
    uint32_t strLength = strlen(pStr);
    buf.push_back(strLength / 256);
    buf.push_back(strLength % 256);
    buf.insert(buf.end(), (uint8_t*)pStr, (uint8_t*)pStr + strLength);
    return strLength + 2;

    // String afterStr;
    // Utils::getHexStrFromBytes(buf.data(), buf.size(), afterStr);
    // LOG_I(MODULE_PREFIX, "bufferAppendString str %s before %s after %s", str.c_str(), beforeStr.c_str(), afterStr.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Check for conn-ack
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool MQTTProtocol::checkForConnAck(const std::vector<uint8_t>& buf, bool& isError)
{
    // Check data length
    if (buf.size() < 4)
    {
        isError = true;
        return false;
    }

    // Check conn-ack msg
    if (((buf[0] & 0xf0) != MQTTCONNACK) || (buf[3] != 0))
    {
        isError = true;
        return false;
    }

    LOG_I(MODULE_PREFIX, "service conn ack ok");
    return true;
}
