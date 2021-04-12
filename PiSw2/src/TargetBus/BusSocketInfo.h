// Bus Raider
// Rob Dobson 2018

#pragma once

#include "BusAccessDefs.h"
#include "TargetCPU.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback types
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback types
typedef void BusAccessCBFnType(void* pObject, uint32_t addr, uint32_t data, uint32_t flags, uint32_t& curRetVal);
typedef void BusReqAckedCBFnType(void* pObject, BR_BUS_REQ_REASON reason, BR_RETURN_TYPE rslt);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Socket Info - this is used to plug-in to the BusAccess layer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusSocketInfo
{
public:
    void setSocketInfo(bool enabled, 
            BusAccessCBFnType* busAccessCallback,
            BusReqAckedCBFnType* busReqAckedCallback,
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
    BusAccessCBFnType* busAccessCallback;
    BusReqAckedCBFnType* busReqAckedCallback;
    void* pSourceObject;

    // Flags
    bool waitOnMemory;
    bool waitOnIO;
};
