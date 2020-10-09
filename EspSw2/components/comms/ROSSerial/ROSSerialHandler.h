/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ROSSerialHandler
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include "ROSTopicManager.h"
#include "Utils.h"

class ROSSerialHandler
{
public:
    static bool getRawDataForTopicIdAndIDNo(uint32_t topicId, uint32_t elemIDNo, uint8_t* pPayload, uint32_t payloadLen,
                uint8_t* pRawData, uint32_t* pRawDataLen, uint32_t maxRawDataLen);
};
