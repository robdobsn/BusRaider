/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerial
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ROSSerialHandler.h"

// #ifdef ROS_SERIAL_EXTRACT_DATA

class ROSSerialAccel
{
public:
    static bool getRawDataForIDNo(int elemIDNo, uint8_t* pPayload, uint32_t payloadLen,
            uint8_t* pRawData, uint32_t* pRawDataLen, uint32_t maxRawDataLen)
    {
        // Check valid
        *pRawDataLen = 0;
        if (payloadLen < ROS_ACCEL_BYTES)
            return false;

        // Search for IDNo
        if ((elemIDNo != -1) && (pPayload[ROS_ACCEL_POS_IDNO] != elemIDNo))
            return false;

        // Extract data
        uint32_t bytesToReturn = ROS_ACCEL_BYTES;
        if (bytesToReturn > maxRawDataLen)
            bytesToReturn = maxRawDataLen;
        memcpy(pRawData, pPayload, bytesToReturn);
        *pRawDataLen = bytesToReturn;
        return true;
    }

#ifdef ROS_SERIAL_EXTRACT_DATA
    ROSSerialAccel()
    {
        IDNo = 0;
        x = 0;
        y = 0;
        z = 0;
    }
    virtual bool extractIDNo(uint8_t* pPayload, uint32_t payloadLen, uint32_t elemIDNo) override final
    {
        // Search for IDNo
        if (payloadLen < ROS_ACCEL_BYTES)
            return false;
        if (pPayload[ROS_ACCEL_POS_IDNO] != elemIDNo)
            return false;
        
        // Extract values
        const uint8_t* pData = pPayload;
        const uint8_t* pEndStop = pData + ROS_ACCEL_BYTES;
        x = Utils::getBEfloat32AndInc(pData, pEndStop);
        y = Utils::getBEfloat32AndInc(pData, pEndStop);
        z = Utils::getBEfloat32AndInc(pData, pEndStop);
        IDNo = Utils::getUint8AndInc(pData, pEndStop);

        // Raw data
        rawData.assign(pPayload, pPayload+ROS_ACCEL_BYTES);
        return true;
    }

public:
    uint16_t IDNo;
    float x;
    float y;
    float z;
#endif
};
