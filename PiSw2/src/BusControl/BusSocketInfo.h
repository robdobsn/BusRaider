// Bus Raider
// Rob Dobson 2018

#pragma once

#include "CPUHandler.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Socket Info - this is used to plug-in to the BusAccess layer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusSocketInfo
{
public:
    void setSocketInfo(bool enabled, 
            CPUCycleCBType* busAccessCallback,
            CPUBusReqCBType* busReqAckedCallback,
            void* pSourceObject)
    {
        // Socket enablement
        this->enabled = enabled;

        // Callbacks
        this->busAccessCallback = busAccessCallback;
        this->busReqAckedCallback = busReqAckedCallback;
        this->pSourceObject = pSourceObject;

        // Flags
        this->waitOnMemory = false;
        this->waitOnIO = false;
    }

    // Socket enablement
    bool enabled;

    // Callbacks
    CPUCycleCBType* busAccessCallback;
    CPUBusReqCBType* busReqAckedCallback;
    void* pSourceObject;

    // Flags
    bool waitOnMemory;
    bool waitOnIO;
};
