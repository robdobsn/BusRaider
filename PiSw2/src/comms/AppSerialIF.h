#pragma once

#include <stdint.h>

class AppSerialIF
{
public:
    AppSerialIF()
    {
    }

    // Handle command
    bool handleRxCmd(const char* pCmdJson, const uint8_t* pParams, unsigned paramsLen,
        char* pRespJson, unsigned maxRespLen)
    {
        return false;
    }
};

