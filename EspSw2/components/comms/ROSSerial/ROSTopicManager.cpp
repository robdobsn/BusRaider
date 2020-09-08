/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSTopicManager
// Some content from Marty V1 rostopics.hpp
//
// Sandy Enoch and Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Logger.h"
#include "ROSSerialMsg.h"
#include "ROSTopicManager.h"
#include "ProtocolEndpointMsg.h"

// Logging
static const char* MODULE_PREFIX = "ROSTopicManager";

const std::vector<ROSTopicInfo> ROSTopicManager::_rosTopics =
    {
        {false, ROSTOPIC_SERVO, "/servo", "marty_msgs/ServoMsg", MD5_ServoMsg, 512},
        {false, ROSTOPIC_SERVOARRAY, "/servo_array", "marty_msgs/ServoMsgArray", MD5_ServoMsgArray, 64},
        {false, ROSTOPIC_ENABLEMOTORS, "/enable_motors", "std_msgs/Bool", MD5_Bool, 64},
        {false, ROSTOPIC_ENABLEMOTORS_ARRAY, "/enable_motors_array", "marty_msgs/ServosEnabled", MD5_ServosEnabled, 64},
        {false, ROSTOPIC_KEYFRAMES, "/keyframes", "marty_msgs/KeyframeArray", MD5_KeyFrameArray, 512},
        {false, ROSTOPIC_SOUND, "/sound", "marty_msgs/SoundArray", MD5_SoundArray, 512},
        {false, ROSTOPIC_SOCKETCMD, "/socket_cmd", "marty_msgs/ByteArray", MD5_ByteArray, 512},
        {true, ROSTOPIC_CHATTER, "/chatter", "std_msgs/String", MD5_String, 512},
        {true, ROSTOPIC_ACCEL, "/accel", "marty_msgs/Accelerometer", MD5_AccelerometerMsg, 64},
        {true, ROSTOPIC_READGPIOS, "/gpios", "marty_msgs/GPIOs", MD5_GpiosMsg, 64},
        {true, ROSTOPIC_MOTORCURRENTS, "/motor_currents", "marty_msgs/MotorCurrents", MD5_MotorCurrentsMsg, 64},
        {true, ROSTOPIC_BATTERY, "/battery", "std_msgs/Float32", MD5_Float32, 64},
        {true, ROSTOPIC_PROX_SENSOR, "/prox_sensor", "std_msgs/Float32", MD5_Float32, 64},
        {true, ROSTOPIC_SERVOPOSITIONS, "/servo_positions", "marty_msgs/ServoPositions", MD5_ServoPositions, 64},
        {true, ROSTOPIC_SERVOSENABLED, "/servos_enabled", "marty_msgs/ServosEnabled", MD5_ServosEnabled, 64}
    };

bool ROSTopicManager::queryROSTopics(ProtocolEndpointMsg& channelMsg, ProtocolEndpointMsgCB msgSendCB)
{
    // Check if we've already sent topic messages
    if (_topicsSent)
    {
        // we only need to reply once
        return false;
    }

    // Generate and send messages
    for (const ROSTopicInfo& tInfo : _rosTopics)
    {
        LOG_D(MODULE_PREFIX, "Sending ROS topic info %s", tInfo.topic_name);

        // Create topic message
        ROSSerialMsg rosMsg;
        uint32_t specialTopic = tInfo._topicIsPublisher ? ROS_SPECIALTOPIC_PUBLISHER : ROS_SPECIALTOPIC_SUBSCRIBER;
        rosMsg.encode(specialTopic, tInfo);

        // Send message
        ProtocolEndpointMsg cmdMsg(channelMsg.getChannelID(), channelMsg.getProtocol(), channelMsg.getMsgNumber(), MSG_DIRECTION_RESPONSE);
        rosMsg.writeRawMsgToVector(cmdMsg.getCmdVector(), false);
        msgSendCB(cmdMsg);
    }

    return true;
}
