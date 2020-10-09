/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerial
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ROSSerialHandler.h"

class ROSSerialRobotStatus
{
public:
    static bool getRawDataForIDNo(int elemIDNo, uint8_t* pPayload, uint32_t payloadLen,
            uint8_t* pRawData, uint32_t* pRawDataLen, uint32_t maxRawDataLen)
    {
        // Check valid
        *pRawDataLen = 0;
        if (payloadLen < ROS_ROBOT_STATUS_BYTES)
            return false;

        // Extract data
        uint32_t bytesToReturn = ROS_ROBOT_STATUS_BYTES;
        if (bytesToReturn > maxRawDataLen)
            bytesToReturn = maxRawDataLen;
        memcpy(pRawData, pPayload, bytesToReturn);
        *pRawDataLen = bytesToReturn;
        return true;
    }

};
