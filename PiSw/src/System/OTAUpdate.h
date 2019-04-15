// Bus Raider
// OTA Update
// Rob Dobson 2019

#pragma once
#include <stdint.h>

class OTAUpdate
{
public:
    static bool performUpdate(const uint8_t* pData, int dataLen);
};
