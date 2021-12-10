/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerial
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ROSSerialHandler.h"

// #ifdef ROS_SERIAL_EXTRACT_DATA

class ROSSerialAddOn
{
public:
    static bool getRawDataForIDNo(int elemIDNo, uint8_t* pPayload, uint32_t payloadLen,
            uint8_t* pRawData, uint32_t* pRawDataLen, uint32_t maxRawDataLen)
    {
        *pRawDataLen = 0;
        // Search for IDNo
        for (uint32_t pos = 0; pos < payloadLen / ROS_ADDON_GROUP_BYTES; pos++)
        {
            // Check IDNo
            uint8_t* pBlock = pPayload+(pos * ROS_ADDON_GROUP_BYTES);
            if (pBlock[ROS_ADDON_IDNO_POS] == elemIDNo)
            {
                // Extract data
                uint32_t bytesToReturn = ROS_ADDON_GROUP_BYTES;
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
    ROSSerialAddOn()
    {
        status = 0;
    }
    virtual bool extractIDNo(uint8_t* pPayload, uint32_t payloadLen, uint32_t elemIDNo) override final
    {
        // Search for IDNo
        for (uint32_t pos = 0; pos < payloadLen / ROS_ADDON_GROUP_BYTES; pos++)
        {
            // Check IDNo
            uint8_t* pBlock = pPayload+pos;
            if (pBlock[ROS_ADDON_IDNO_POS] == elemIDNo)
            {
                // Extract values
                const uint8_t* pData = pBlock;
                const uint8_t* pEndStop = pData + ROS_ADDON_GROUP_BYTES;
                IDNo = Utils::getUint8AndInc(pData, pEndStop);
                status = Utils::getUint8AndInc(pData, pEndStop);

                // Raw data
                rawData.assign(pData, pData+ROS_ADDON_STATUS_BYTES);
                return true;
            }
        }
        return false;
    }

public:
    uint8_t status;
#endif
};
