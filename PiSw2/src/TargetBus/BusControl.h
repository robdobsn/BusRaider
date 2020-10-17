// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include "lowlib.h"
#include "lowlev.h"
#include "TargetCPU.h"
#include "TargetController.h"
#include "BusSocketManager.h"
#include "BusAccessDefs.h"
#include "TargetClockGenerator.h"
#include "BusAccessStatusInfo.h"
#include "MemoryController.h"
#include "BusRawAccess.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusControl
{
public:
    BusControl();

    // Initialization
    void init();
    void machineChangeInit();

    // Service
    void service();

    // Raw access start/end
    void rawAccessStart();
    void rawAccessEnd();

    // Target controller
    TargetController& ctrl()
    {
        return _targetController;
    }    

    // Target programmer
    TargetProgrammer& prog()
    {
        return _targetController.targetProgrammer();
    }    

    // Bus socket manager
    BusSocketManager& sock()
    {
        return _busSocketManager;
    }

    // Target clock generator
    TargetClockGenerator& clock()
    {
        return _clockGenerator;
    }

    // Memory controller
    MemoryController& mem()
    {
        return _memoryController;
    }

    // Raw bus access
    BusRawAccess& bus()
    {
        return _busRawAccess;
    }

    // Get return type string
    const char* retcString(BR_RETURN_TYPE retc)
    {
        switch (retc)
        {
            case BR_OK: return "ok";
            case BR_ERR: return "general error";
            case BR_ALREADY_DONE: return "already paused";
            case BR_NO_BUS_ACK: return "BUSACK not received";
            default: return "unknown error";
        }
    }
    
private:
    // State of bus
    bool _isInitialized;

    // Clock generator
    TargetClockGenerator _clockGenerator;

    // TargetController
    TargetController _targetController;

    // Bus Sockets
    BusSocketManager _busSocketManager;

    // MemoryController
    MemoryController _memoryController;

    // Bus raw access
    BusRawAccess _busRawAccess;
};