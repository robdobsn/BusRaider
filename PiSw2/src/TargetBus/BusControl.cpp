// Bus Raider
// Rob Dobson 2018-2020

#include "BusControl.h"
#include "PiWiring.h"
#include <circle/interrupt.h>
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "logging.h"
#include "circle/timer.h"
#include "DebugHelper.h"

// Module name
static const char MODULE_PREFIX[] = "BusControl";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusControl::BusControl()
    : _targetControl(*this), 
      _busSocketManager(*this), 
      _memoryController(*this),
      _busRawAccess(*this, _clockGenerator, 
                    [](void* pObj){ 
                        if(pObj) 
                            ((BusControl*)pObj)->_targetControl.service(true); 
                        },
                    this),
      _hwManager(*this)
{
    // Not init yet
    _isInitialized = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise the hardware
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::init()
{
    if (!_isInitialized)
    {
        // Clock
        _clockGenerator.setup();
        _clockGenerator.setFreqHz(1000000);
        _clockGenerator.enable(true);
        
        // Handlers
        _busRawAccess.init();
        _targetControl.init();
        _busSocketManager.init();

        // Now initialized
        _isInitialized = true;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::service()
{
    // Service raw access
    _busRawAccess.service();

    // Service target controller
    _targetControl.service(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Machine changes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::machineChangeInit()
{
    // Suspend socket manager
    _busSocketManager.suspend(true, true);

    // Clear the target controller
    _targetControl.clear();
}

void BusControl::machineChangeComplete()
{
    // Resume socket manager
    _busSocketManager.suspend(false, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Raw access start/end
// Used by self-test to suspend normal bus operation (wait handling etc)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusControl::rawAccessStart()
{
    _busSocketManager.suspend(true, true);
    _targetControl.suspend(true);
}

void BusControl::rawAccessEnd()
{
    _busSocketManager.suspend(false, true);
    _targetControl.suspend(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Block access synchronous
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE BusControl::blockAccessSync(uint32_t addr, uint8_t* pData, uint32_t len, bool iorq, 
            bool read, bool write)
{
    // Check if we are in debug mode - if so access will be via copy of memory
    if (!isDebugging())
    {
        // Assert BUSRQ and wait for the bus
        BR_RETURN_TYPE retc = _busRawAccess.busRequestAndTake();
        if (retc != BR_OK)
            return retc;
    }

    // Access the block (can both write and read)
    BR_RETURN_TYPE retc = BR_OK;
    if (write)
        retc =_memoryController.blockWrite(addr, pData, 
                 len, iorq ? BLOCK_ACCESS_IO : BLOCK_ACCESS_MEM);
    if (read && (retc == BR_OK))
        retc = _memoryController.blockRead(addr, pData, 
                 len, iorq ? BLOCK_ACCESS_IO : BLOCK_ACCESS_MEM);

    // Check if we are in debug mode again
    if (!isDebugging())
    {
        // Release BUSRQ
        _busRawAccess.busReqRelease();
    }

    return retc;
}

