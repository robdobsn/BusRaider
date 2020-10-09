/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerial
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ROSSerialHandler.h"

// #ifdef ROS_SERIAL_EXTRACT_DATA

class ROSSerialSmartServo
{
public:
    static bool getRawDataForIDNo(int elemIDNo, uint8_t* pPayload, uint32_t payloadLen,
            uint8_t* pRawData, uint32_t* pRawDataLen, uint32_t maxRawDataLen)
    {
        *pRawDataLen = 0;
        // Search for IDNo
        for (uint32_t pos = 0; pos < payloadLen / ROS_SMART_SERVOS_ATTR_GROUP_BYTES; pos++)
        {
            // Check IDNo
            uint8_t* pBlock = pPayload+pos;
            if (pBlock[ROS_SMART_SERVOS_ATTR_ID_IDX] == elemIDNo)
            {
                // Extract data
                uint32_t bytesToReturn = ROS_SMART_SERVOS_ATTR_GROUP_BYTES;
                if (bytesToReturn > maxRawDataLen)
                    bytesToReturn = maxRawDataLen;
                memcpy(pRawData, pBlock, bytesToReturn);
                *pRawDataLen = bytesToReturn;
                return true;
            }
        }
        return false;
    }

#ifdef ROS_SERIAL_EXTRACT_DATA
    ROSSerialSmartServo()
    {
        servoPos = 0;
        currentMA = 0;
        status = 0;
    }
    virtual bool extractIDNo(uint8_t* pPayload, uint32_t payloadLen, uint32_t elemIDNo) override final
    {
        // Search for IDNo
        for (uint32_t pos = 0; pos < payloadLen / ROS_SMART_SERVOS_ATTR_GROUP_BYTES; pos++)
        {
            // Check IDNo
            uint8_t* pBlock = pPayload+pos;
            if (pBlock[ROS_SMART_SERVOS_ATTR_ID_IDX] == elemIDNo)
            {
                // Extract values
                const uint8_t* pData = pBlock;
                const uint8_t* pEndStop = pData + ROS_SMART_SERVOS_ATTR_GROUP_BYTES;
                IDNo = Utils::getUint8AndInc(pData, pEndStop);
                servoPos = Utils::getBEInt16AndInc(pData, pEndStop);
                currentMA = Utils::getBEInt16AndInc(pData, pEndStop);
                status = Utils::getUint8AndInc(pData, pEndStop);

                // Raw data
                rawData.assign(pBlock, pBlock+ROS_SMART_SERVOS_ATTR_GROUP_BYTES);
                return true;
            }
        }
        return false;
    }

public:
    int servoPos;
    int currentMA;
    uint8_t status;
#endif
};
