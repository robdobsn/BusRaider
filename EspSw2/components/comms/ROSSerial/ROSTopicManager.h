/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSTopicManager
// Some content from Marty V1 rostopics.hpp
//
// Sandy Enoch and Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <vector>
#include <ProtocolBase.h>

#define ROSTOPIC_SERVO              102
#define ROSTOPIC_SERVOARRAY         103
#define ROSTOPIC_CHATTER            101
#define ROSTOPIC_ACCEL              104
#define ROSTOPIC_ENABLEMOTORS       105
#define ROSTOPIC_READGPIOS          106
#define ROSTOPIC_BATTERY            107
#define ROSTOPIC_MOTORCURRENTS      108
#define ROSTOPIC_SOUND              109
#define ROSTOPIC_KEYFRAMES          110
// the next one is used by the ESP etc. as a spoof rostopic,
// we use rosserial format to encode the payload length, so it doesn't follow strict ROS convention
#define ROSTOPIC_CMD                111
// this is the actual ROS topic that will be advertised and listened to
// it exposes the socket API to ROS
#define ROSTOPIC_SOCKETCMD          112

#define ROSTOPIC_SERVOPOSITIONS     113
#define ROSTOPIC_SERVOSENABLED      114
#define ROSTOPIC_ENABLEMOTORS_ARRAY 115
#define ROSTOPIC_PROX_SENSOR        116

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// New ROSTOPICS for V2
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROSTOPIC_V2_SMART_SERVOS    120
#define ROSTOPIC_V2_ACCEL           121
#define ROSTOPIC_V2_POWER_STATUS    122
#define ROSTOPIC_V2_ADDONS          123
#define ROSTOPIC_V2_ROBOT_STATUS    124

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC SMART_SERVOS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_SMART_SERVOS_MAX_NUM_SERVOS 15
#define ROS_SMART_SERVOS_ATTR_GROUP_BYTES 6
#define ROS_SMART_SERVOS_ATTR_ID_IDX 0
#define ROS_SMART_SERVOS_ATTR_SERVO_POS_IDX 1
#define ROS_SMART_SERVOS_ATTR_MOTOR_CURRENT_IDX 3
#define ROS_SMART_SERVOS_ATTR_STATUS_IDX 5
#define ROS_SMART_SERVOS_INVALID_POS -32768
#define ROS_SMART_SERVOS_INVALID_CURRENT -32768

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC ACCEL message layout
//
// float32 x
// float32 y
// float32 z
// uint8_t IDNo
// uint8_t Bit 0 == freefall flag
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_ACCEL_BYTES 14
#define ROS_ACCEL_POS_X 0
#define ROS_ACCEL_POS_Y 4
#define ROS_ACCEL_POS_Z 8
#define ROS_ACCEL_POS_IDNO 12
#define ROS_ACCEL_POS_FLAGS 13

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC POWER STATUS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_POWER_STATUS_BYTES 13
#define ROS_POWER_STATUS_REMCAPPC 0
#define ROS_POWER_STATUS_BATTTEMPC 1
#define ROS_POWER_STATUS_REMCAPMAH 2
#define ROS_POWER_STATUS_FULLCAPMAH 4
#define ROS_POWER_STATUS_CURRENTMA 6
#define ROS_POWER_STATUS_5V_ON_SECS 8
#define ROS_POWER_STATUS_POWERFLAGS 10
#define ROS_POWER_STATUS_FLAGBIT_USB_POWER 0
#define ROS_POWER_STATUS_FLAGBIT_5V_ON 1
#define ROS_POWER_STATUS_IDNO 12

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC ROBOT STATUS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_ROBOT_STATUS_BYTES 2
#define ROS_ROBOT_STATUS_MOTION_FLAGS 0
#define ROS_ROBOT_STATUS_IS_MOVING_MASK 0x01
#define ROS_ROBOT_STATUS_IS_PAUSED_MASK 0x02
#define ROS_ROBOT_STATUS_FW_UPDATE_MASK 0x04
#define ROS_ROBOT_STATUS_QUEUED_WORK_COUNT 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V2 ROSTOPIC ADDONS message layout
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROS_ADDONS_MAX_NUM_ADDONS 15
#define ROS_ADDON_GROUP_BYTES 12
#define ROS_ADDON_IDNO_POS 0
#define ROS_ADDON_FLAGS_POS 1
#define ROS_ADDON_DATAVALID_FLAG_MASK 0x80
#define ROS_ADDON_STATUS_POS 2
#define ROS_ADDON_STATUS_BYTES 10

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// V1 ROSTOPICS
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ROSTOPIC_WIFIRESET          200

#define CMD_GETREADY                0
#define CMD_MOVEJOINT               1
#define CMD_LEAN                    2
#define CMD_WALK                    3
#define CMD_EYES                    4
#define CMD_KICK                    5
#define CMD_CELEBRATE               8
#define CMD_TAP_FOOT                 10
#define CMD_ARMS                    11
#define CMD_SIDESTEP                14
#define CMD_STANDSTRAIGHT           15
#define CMD_SOUND                   16
#define CMD_STOP                    17
#define CMD_MOVEJOINTID             18
#define CMD_ENABLEMOTORS            19
#define CMD_DISABLEMOTORS           20
#define CMD_FALLPROTECTION          21
#define CMD_MOTORPROTECTION         22
#define CMD_LOWBATTERYWARNING       23
#define CMD_BUZZPREVENTION          24
#define CMD_SET_IO_TYPE             25
#define CMD_IO_WRITE                26
#define CMD_I2C_WRITE               27
#define CMD_CIRCLEDANCE             28
#define CMD_LIFELIKEBEHAVIOUR       29
#define CMD_ENABLESAFETIES          30
#define CMD_SET_PARAMETER           31
#define CMD_GET_FIRMWARE_VERSION    32
#define CMD_MUTE_ESP_SERIAL         33
#define CMD_CLEARCALIBRATION        254
#define CMD_SAVECALIBRATION         255

// parameters
#define CMD_PARAM_LEANANGLE         0
#define CMD_PARAM_FALLTHRESHOLD     1
#define CMD_PARAM_PERIOD            2
#define CMD_PARAM_SERVOTHRESH_INST  3
#define CMD_PARAM_SERVOTHRESH_LI    4
#define CMD_PARAM_SERVO_MULT		5
#define CMD_PARAM_SERVO_BUZZ_MIN	6
#define CMD_PARAM_SERVO_BUZZ_MAX	7

#define CMD_MOVEJOINT_HIP           0
#define CMD_MOVEJOINT_TWIST         1
#define CMD_MOVEJOINT_KNEE          2
#define CMD_MOVEJOINT_ARM           3
#define CMD_MOVEJOINT_EYES          4

// stop types
#define CMD_STOP_MOVEQUEUE_ONLY     0
#define CMD_STOP_FREEZE             1
#define CMD_STOP_DISABLE_MOTORS     2
#define CMD_STOP_RETURN_TO_ZERO     3
#define CMD_STOP_PAUSE              4
#define CMD_STOP_PAUSE_AND_DISABLE  5

// disable servo types
#define CMD_DISABLE_INSTANT         0
#define CMD_DISABLE_AFTER_MOVEQUEUE 1
#define CMD_ENABLE_INSTANT          0
#define CMD_ENABLE_AFTER_MOVEQUEUE  1

// get ready options
#define CMD_GETREADY_ENABLE_MOTORS  1

// IO pin types
#define CMD_IO_DIGITALIN            0
#define CMD_IO_ANALOGIN             1
#define CMD_IO_DIGITALOUT           2
#define CMD_IO_SERVO                3
#define CMD_IO_PWM                  4
// TODO: sonar, neopixel, others?

// sanity checks
#define CMD_MAX_STEPS               10
#define CMD_MAX_SOUND_DURATION      5000        // in ms.

// default periods
#define ROSTOPIC_READGPIOS_PERIOD   100         // period in ms.
#define ROSTOPIC_ACC_PERIOD       100
#define ROSTOPIC_BATTERY_PERIOD     100
#define ROSTOPIC_MOTORCURRENTS_PERIOD   100
#define ROSTOPIC_SERVOPOSITIONS_PERIOD  100
#define ROSTOPIC_TIME_PERIOD        1000
#define ROSTOPIC_PROX_SENSOR_PERIOD    100

#define ROSTOPIC_PERIOD_MIN     10

#define MD5_Int8 "27ffa0c9c4b8fb8492252bcad9e5c57b"
#define MD5_String "992ce8a1687cec8c8bd883ec73ca41d1"
#define MD5_Int32 "da5909fbe378aeaf85e547e830cc1bb7"
#define MD5_Servos "da5909fbe378aeaf85e547e830cc1bb7"
#define MD5_ServoMsg "1bdf7d827c361b33ed3b4360c70ce4c0"
#define MD5_ServoMsgArray "320c8def1674d51423fb942c56a6619b"
#define MD5_AccelerometerMsg "cc153912f1453b708d221682bc23d9ac"
#define MD5_Bool "8b94c1b53db61fb6aed406028ad6332a"
#define MD5_Float32 "73fcbf46b49191e672908e50842a83d4"
#define MD5_GpiosMsg "ae1841d9445eabd94be4a4db108239e6"
#define MD5_MotorCurrentsMsg "de3564b1a76cacf83b6bf2f9b551103d"
#define MD5_Sound "358e233cde0c8a8bcfea4ce193f8fc15"
#define MD5_SoundArray "3c33c083882cb63e74c515223d270391"
#define MD5_KeyFrameArray "4063443b99a72161044d8ebb68be9334"
#define MD5_ByteArray "5a7f134de1b7437eae7d27ef3b79b5b2"
#define MD5_ServoPositions "1e3e1c4f9a16b054f8d9a8b4e96f272f"
#define MD5_ServosEnabled   "990297478fec17b96fdc1a726bc998b9"

class ROSTopicInfo
{
public:
    bool _topicIsPublisher;
    // Don't change these types as they are used in sizeof to setup messages
    uint16_t topic_id;
    const char *topic_name;
    const char *message_type;
    const char *md5sum;
    int32_t buffer_size;

};

class ROSTopicManager
{
public:
    ROSTopicManager()
    {
        _topicsSent = false;
    }
    bool queryROSTopics(ProtocolEndpointMsg& channelMsg, ProtocolEndpointMsgCB msgSendCB);

private:
    bool _topicsSent;

    static const std::vector<ROSTopicInfo> _rosTopics;
};

// typedef struct {
//     uint16_t gpio;
//     uint16_t motor_current;
//     uint16_t battery;
//     uint16_t acc;
//     uint16_t servo_position;
//     uint16_t prox_sensor;
// } Rostopic_periods;

// extern  Rostopic_periods rostopic_periods;
