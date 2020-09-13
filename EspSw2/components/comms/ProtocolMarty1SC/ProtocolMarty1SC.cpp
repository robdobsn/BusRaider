/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ProtocolMarty1SC
// Protocol wrapper implementing Marty1ShortCodes
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ProtocolMarty1SC.h"
#include <ProtocolEndpointMsg.h>
#include <ROSSerialMsg.h>
#include <ConfigBase.h>

// Logging
static const char* MODULE_PREFIX = "PcolMarty1SC";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProtocolMarty1SC::ProtocolMarty1SC(uint32_t channelID, const char* configJSON, ProtocolEndpointMsgCB msgTxCB, ProtocolEndpointMsgCB msgRxCB) :
    ProtocolBase(channelID), _msgTxCB(msgTxCB), _msgRxCB(msgRxCB)
{
    // Extract configuration
    ConfigBase config(configJSON);
    _maxRxMsgLen = config.getLong("MaxRxMsgLen", DEFAULT_MARTY1SC_RX_MAX);
    _maxTxMsgLen = config.getLong("MaxTxMsgLen", DEFAULT_MARTY1SC_TX_MAX);

    // Init command decoder
    _pCmdDecoder = new ProtocolMarty1SCDecoder(0, _maxRxMsgLen);
    if (_pCmdDecoder)
    {
        _pCmdDecoder->stdMsgRxCallback = std::bind(&ProtocolMarty1SC::stdMsgCallback, this, std::placeholders::_1, std::placeholders::_2);
        _pCmdDecoder->rosMsgRxCallback = std::bind(&ProtocolMarty1SC::rosMsgCallback, this, std::placeholders::_1, std::placeholders::_2);
        _pCmdDecoder->sensorMsgCallback = std::bind(&ProtocolMarty1SC::sensorMsgCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    }
}

ProtocolMarty1SC::~ProtocolMarty1SC()
{
    if (_pCmdDecoder)
        delete _pCmdDecoder;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// addRxData
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolMarty1SC::addRxData(const uint8_t* pData, uint32_t dataLen)
{
    // Debug
    LOG_V(MODULE_PREFIX, "addRxData len %d", dataLen);

    // Check CommandDecoder
    if (!_pCmdDecoder)
        return;

    // Push into cmdDecoder
    _pCmdDecoder->feed(pData, dataLen);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// encodeTxMsgAndSend
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolMarty1SC::encodeTxMsgAndSend(ProtocolEndpointMsg& msg)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rx Handlers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProtocolMarty1SC::stdMsgCallback(int id, size_t length)
{
    // Debug
    LOG_V(MODULE_PREFIX, "stdMsgCallback len %d", length);
    if (!_pCmdDecoder)
        return;
    if (!_msgRxCB)
        return;

    // The following from esp_firmware.ino in original Marty1 esp-firmware 
    // TODO - fixed length buffer - ensure length ok
    static const int MAX_DEST_MSG_LEN = 100;
    uint8_t command_buf[MAX_DEST_MSG_LEN];
    _pCmdDecoder->read(command_buf+7, length);
    const uint16_t topic_id = 111;

    command_buf[ROSSERIAL_SYNC_FLAG_POS] = ROSSERIAL_SYNC_FLAG_VAL;
    command_buf[ROSSERIAL_PCOL_FLAG_POS] = ROSSERIAL_PCOL_FLAG_VAL;

    command_buf[ROSSERIAL_MSG_LEN_LOW_POS] = length & 0xFF;
    command_buf[ROSSERIAL_MSG_LEN_HIGH_POS] = (length >> 8) & 0xFF;

    command_buf[ROSSERIAL_MSG_LEN_CSUM_POS] = checksum1((uint16_t) length);

    command_buf[ROSSERIAL_MSG_TOPIC_ID_LOW_POS] = topic_id & 0xFF;
    command_buf[ROSSERIAL_MSG_TOPIC_ID_HIGH_POS] = (topic_id >> 8) & 0xFF; 

    command_buf[length + ROSSERIAL_MSG_PAYLOAD_CSUM_OFFSET] = checksum2(topic_id, command_buf + ROSSERIAL_MSG_PAYLOAD_POS, length);

    // Send as ROS message in ProtocolEndpointMsg
    ProtocolEndpointMsg endpointMsg;
    endpointMsg.setFromBuffer(_channelID, MSG_PROTOCOL_ROSSERIAL,
                PROTOCOL_MSG_UNNUMBERED_NUM, MSG_DIRECTION_COMMAND, command_buf, length + ROSSERIAL_MSG_MIN_LEN);
    _msgRxCB(endpointMsg);
}

void ProtocolMarty1SC::rosMsgCallback(int id, size_t length)
{
    // Debug
    LOG_V(MODULE_PREFIX, "rosMsgCallback len %d", length);
    if (!_pCmdDecoder)
        return;
    // TODO - fixed length buffer - ensure length ok
    static const int MAX_DEST_MSG_LEN = 100;
    uint8_t command_buf[MAX_DEST_MSG_LEN];
    _pCmdDecoder->read(command_buf, length);

    // Send as ROS message in ProtocolEndpointMsg
    ProtocolEndpointMsg endpointMsg;
    endpointMsg.setFromBuffer(_channelID, MSG_PROTOCOL_ROSSERIAL,
                PROTOCOL_MSG_UNNUMBERED_NUM, MSG_DIRECTION_COMMAND, command_buf, length);
    _msgRxCB(endpointMsg);
}

void ProtocolMarty1SC::sensorMsgCallback(int id, uint8_t sensor, uint8_t sensor_id)
{
    // Debug
    // TODO - implement
    LOG_D(MODULE_PREFIX, "sensorMsgCallback sensor %d sensorId %d", sensor, sensor_id);

    // static const int MAX_DEST_MSG_LEN = 10;
    // uint8_t command_buf[MAX_DEST_MSG_LEN];
    // float* ptr = (float*) command_buf;
    // switch (sensor){
    //     case GET_CHATTER:
    //         managed_socket_clients[id].write((uint8_t*)&ros_decoder.chatter_length, 1);
    //         managed_socket_clients[id].write((uint8_t*)&ros_decoder.chatter, ros_decoder.chatter_length);
    //         break;
    //     case GET_SERVO_POSITION:
    //     {
    //         int8_t servopos = 0;
    //         if (sensor_id < NUM_SERVOS){
    //             servopos = ros_decoder.servo_positions[sensor_id];
    //         }
    //         managed_socket_clients[id].write((uint8_t*)&servopos, 1);
    //         break;
    //     }
    //     case GET_SERVO_ENABLED:
    //     {
    //         uint8_t servo_enabled = 0;
    //         if (sensor_id < NUM_SERVOS){
    //             servo_enabled = ros_decoder.servos_enabled[sensor_id];
    //         }
    //         managed_socket_clients[id].write((uint8_t*)&servo_enabled, 1);
    //         break;
    //     }
    //     default:
    //         *ptr = decodeGetPacket(sensor, sensor_id);
    //         managed_socket_clients[id].write((const uint8_t*)command_buf, 4);
    // }
}
