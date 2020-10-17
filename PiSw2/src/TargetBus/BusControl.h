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
#include "BusSocketInfo.h"
#include "BusAccessDefs.h"
#include "TargetClockGenerator.h"
#include "BusAccessStatusInfo.h"

// #define ISR_TEST 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusControl
{
public:
    BusControl();

    init();
    
private:
    // State of bus
    bool _isInitialized;
    bool _busReqAcknowledged;
    bool _waitIsActive;
    bool _pageIsActive;

    // Helpers
    void waitSystemInit();
};
