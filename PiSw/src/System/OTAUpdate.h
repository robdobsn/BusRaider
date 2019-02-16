// Bus Raider
// OTA Update
// Rob Dobson 2019

#pragma once
#include <stdint.h>

class OTAUpdate
{
public:
    static void performUpdate(uint8_t* pData, int dataLen);
};
