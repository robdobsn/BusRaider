/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerialBase
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ROSSerialAccel.h"
#include "ROSSerialAddOn.h"
#include "ROSSerialSmartServo.h"
#include "ROSSerialPowerStatus.h"
#include "ROSSerialRobotStatus.h"

bool ROSSerialHandler::getRawDataForTopicIdAndIDNo(uint32_t topicId, uint32_t elemIDNo, uint8_t* pPayload, uint32_t payloadLen,
            uint8_t* pRawData, uint32_t* pRawDataLen, uint32_t maxRawDataLen)
{
    switch(topicId)
    {
        case ROSTOPIC_V2_SMART_SERVOS:
            return ROSSerialSmartServo::getRawDataForIDNo(elemIDNo, pPayload, payloadLen, 
                                pRawData, pRawDataLen, maxRawDataLen);
        case ROSTOPIC_V2_ACCEL:
            return ROSSerialAccel::getRawDataForIDNo(elemIDNo, pPayload, payloadLen, 
                                pRawData, pRawDataLen, maxRawDataLen);
        case ROSTOPIC_V2_POWER_STATUS:
            return ROSSerialPowerStatus::getRawDataForIDNo(elemIDNo, pPayload, payloadLen, 
                                pRawData, pRawDataLen, maxRawDataLen);
        case ROSTOPIC_V2_ADDONS:
            return ROSSerialAddOn::getRawDataForIDNo(elemIDNo, pPayload, payloadLen, 
                                pRawData, pRawDataLen, maxRawDataLen);
        case ROSTOPIC_V2_ROBOT_STATUS:
            // Robot status doesn't have an IDNo
            return false;
        default:
            return false;
    }
}

