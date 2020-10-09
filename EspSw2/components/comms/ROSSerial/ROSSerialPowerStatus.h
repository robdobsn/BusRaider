/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerial
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ROSSerialHandler.h"

// #ifdef ROS_SERIAL_EXTRACT_DATA

class ROSSerialPowerStatus
{
public:
    static bool getRawDataForIDNo(int elemIDNo, uint8_t* pPayload, uint32_t payloadLen,
            uint8_t* pRawData, uint32_t* pRawDataLen, uint32_t maxRawDataLen)
    {
        // Check valid
        *pRawDataLen = 0;
        if (payloadLen < ROS_POWER_STATUS_BYTES)
            return false;

        // Search for IDNo
        if ((elemIDNo != -1) && (pPayload[ROS_POWER_STATUS_IDNO] != elemIDNo))
            return false;

        // Extract data
        uint32_t bytesToReturn = ROS_POWER_STATUS_BYTES;
        if (bytesToReturn > maxRawDataLen)
            bytesToReturn = maxRawDataLen;
        memcpy(pRawData, pPayload, bytesToReturn);
        *pRawDataLen = bytesToReturn;
        return true;
    }

#ifdef ROS_SERIAL_EXTRACT_DATA
    ROSSerialPowerStatus()
    {
        remCapPC = 0;
        tempDegC = 0;
        remCapMAH = 0;
        fullCapMAH = 0;
        currentMA = 0;
        powerOnTimeSecs = 0;
        statusFlags = 0;
    }
    virtual bool extractIDNo(uint8_t* pPayload, uint32_t payloadLen, uint32_t elemIDNo) override final
    {
        // Search for IDNo
        if (payloadLen < ROS_POWER_STATUS_BYTES)
            return false;
        if (pPayload[ROS_POWER_STATUS_IDNO] != elemIDNo)
            return false;
        
        // Extract values
        const uint8_t* pData = pPayload;
        const uint8_t* pEndStop = pData + ROS_POWER_STATUS_BYTES;

        // Get from message
        remCapPC = Utils::getUint8AndInc(pData, pEndStop);
        tempDegC = Utils::getUint8AndInc(pData, pEndStop);
        remCapMAH = Utils::getBEUint16AndInc(pData, pEndStop);
        fullCapMAH = Utils::getBEUint16AndInc(pData, pEndStop);
        currentMA = Utils::getBEInt16AndInc(pData, pEndStop);
        powerOnTimeSecs = Utils::getBEUint16AndInc(pData, pEndStop);
        statusFlags = Utils::getBEUint16AndInc(pData, pEndStop);
        IDNo = Utils::getUint8AndInc(pData, pEndStop);

        // Raw data
        rawData.assign(pPayload, pPayload+ROS_POWER_STATUS_BYTES);
        return true;
    }

public:
    uint8_t remCapPC;
    uint8_t tempDegC;
    uint16_t remCapMAH;
    uint16_t fullCapMAH;
    int16_t currentMA;
    uint16_t powerOnTimeSecs;
    uint16_t statusFlags;
#endif
};
