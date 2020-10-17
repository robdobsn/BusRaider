// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>

#define BUS_CYCLE_SERVICE_LOOPS 50

// Module name
static const char MODULE_PREFIX[] = "BusAccCycle";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sevice bus activity
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::busCycleService()
{
    uint32_t serviceLoopStartUs = micros();

    // Check for new bus actions
    busActionCheck();

    // Handle completion of any existing bus actions
    busActionHandleActive();

    waitRelease();

    return;

    // Loop repeatedly to service waits
    for (int i = 0; i < BUS_CYCLE_SERVICE_LOOPS; i++)
    {

        // // Read bus
        // uint32_t busVals = read32(ARM_GPIO_GPLEV0);

        // // Check we're not already in a wait
        // if (!waitAsserted)
        // {
        //     // Check for new wait
        //     if (newWaitOccurred)
        //     {
        //         // Check address
                
        //         handleNewWait();
        //     }

        //     // 
        // }

        // Check if we are already in a wait
        if (!_waitIsActive)
        {
            // We're not currently in wait so read the control lines to check for a new one
            uint32_t busVals = read32(ARM_GPIO_GPLEV0);

            // Check if we have a new wait (and we're not in BUSACK)
            if (((busVals & BR_WAIT_BAR_MASK) == 0) && ((busVals & BR_BUSACK_BAR_MASK) != 0))
            {
                // Record the time of the wait start
                _waitAssertedStartUs = micros();
                _waitIsActive = true;

                // Handle the wait
                cycleHandleNewWait();
            }
            else
            {
                // Check if we're waiting on memory accesses
                if (!waitIsOnMemory())
                {
                    // We're not waiting on memory accesses so it is safe to check
                    // if we need to assert any new bus requests
                    busActionHandleStart();
                }
            }
        }

        // Handle existing waits (including ones just started)
        if (_waitIsActive)
        {
            // Read the control lines
            uint32_t busVals = read32(ARM_GPIO_GPLEV0);

            // Check if wait can be released
            bool releaseWait = true;
            if (((busVals & BR_MREQ_BAR_MASK) == 0) && _waitHold)
            {
                releaseWait = false;
                // Extend timing of bus actions in this case
                _busActionInProgressStartUs = micros();
                _busActionAssertedStartUs = micros();
            }

            // Release the wait if timed-out
            if (releaseWait && isTimeout(micros(), _waitAssertedStartUs, _waitCycleLengthUs))
            {
                // Check if we need to assert any new bus requests
                busActionHandleStart();

                // Release the wait
                // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitRelease due to timeout, waitHold %d busVals %x", _waitHold, busVals);

                waitRelease();
            }
        }

        // // Check 
        // if ((_busActionState != BUS_ACTION_STATE_ASSERTED) && (!_waitIsActive))
        //     break;

        if (isTimeout(micros(), serviceLoopStartUs, BR_MAX_TIME_IN_SERVICE_LOOP_US))
            break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// New Wait
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::cycleHandleNewWait()
{
    // Time at start of ISR
    uint32_t isrStartUs = micros();

    uint32_t addr = 0;
    uint32_t dataBusVals = 0;

    // Check if paging in/out is required
    if (_targetPageInOnReadComplete)
    {
        busAccessCallbackPageIn();
        _targetPageInOnReadComplete = false;
    }

    // Read control lines
    uint32_t ctrlBusVals = controlBusRead();
    
    // Check if bus detail is suspended for one cycle
    if (_waitSuspendBusDetailOneCycle)
    {
            //         digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            // lowlev_cycleDelay(20);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        _waitSuspendBusDetailOneCycle = false;
    }
    else
    {

        // TODO DEBUG
            // pinMode(BR_WR_BAR, INPUT);
            // pinMode(BR_RD_BAR, INPUT);
            // pinMode(BR_MREQ_BAR, INPUT);
            // pinMode(BR_IORQ_BAR, INPUT);

        addrAndDataBusRead(addr, dataBusVals);
    }

    // Send this to all bus sockets
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    for (int sockIdx = 0; sockIdx < _busSocketCount; sockIdx++)
    {
        if (_busSockets[sockIdx].enabled && _busSockets[sockIdx].busAccessCallback)
        {
            _busSockets[sockIdx].busAccessCallback(_busSockets[sockIdx].pSourceObject,
                             addr, dataBusVals, ctrlBusVals, retVal);
            // TODO
            // if (ctrlBusVals & BR_CTRL_BUS_IORQ_MASK)
            //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "%d IORQ %s from %04x %02x", sockIdx,
            //             (ctrlBusVals & BR_CTRL_BUS_RD_MASK) ? "RD" : ((ctrlBusVals & BR_CTRL_BUS_WR_MASK) ? "WR" : "??"),
            //             addr, 
            //             (ctrlBusVals & BR_CTRL_BUS_WR_MASK) ? dataBusVals : retVal);
        }
    }

#ifdef DEBUG_IORQ_PROCESSING

    // TEST CODE
    // TODO
    if ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) && (!(ctrlBusVals & BR_CTRL_BUS_M1_MASK)))
    {
        if ((_statusInfo._debugIORQIntProcessed) && (!_statusInfo._debugIORQIntNext))
        {
            ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_J);
            // Find match if any
            bool matchFound = false;
            // for (int i = 0; i < _statusInfo._debugIORQNum; i++)
            // {
            //     if (_statusInfo._debugIORQEvs[i]._addr == (addr & 0xff))
            //     {
            //         matchFound = true;
            //         _statusInfo._debugIORQEvs[i]._count++;
            //         break;
            //     }
            // }
            if ((!matchFound) && (_statusInfo._debugIORQNum < _statusInfo.MAX_DEBUG_IORQ_EVS))
            {
                _statusInfo._debugIORQEvs[_statusInfo._debugIORQNum]._micros = micros();
                _statusInfo._debugIORQEvs[_statusInfo._debugIORQNum]._addr = (addr & 0xff);
                _statusInfo._debugIORQEvs[_statusInfo._debugIORQNum]._data = dataBusVals;
                _statusInfo._debugIORQEvs[_statusInfo._debugIORQNum]._flags = ctrlBusVals;
                _statusInfo._debugIORQEvs[_statusInfo._debugIORQNum]._count = 1;
                _statusInfo._debugIORQNum++;
                                    ISR_VALUE(ISR_ASSERT_CODE_DEBUG_K, _statusInfo._debugIORQNum);

            }

            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            // lowlev_cycleDelay(20);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        }
        else if (_statusInfo._debugIORQIntNext)
        {
            if ((_statusInfo._debugIORQMatchNum < _statusInfo.MAX_DEBUG_IORQ_EVS) && (_statusInfo._debugIORQMatchNum < _statusInfo._debugIORQNum))
            {
                _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._micros = micros();
                _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._addr = (addr & 0xff);
                _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._data = dataBusVals;
                _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._flags = ctrlBusVals;
                _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._count = 1;

                if ((_statusInfo._debugIORQEvs[_statusInfo._debugIORQMatchNum]._addr != _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._addr) ||
                    (_statusInfo._debugIORQEvs[_statusInfo._debugIORQMatchNum]._flags != _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._flags))
                {
                    // TODO
                    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
                    // lowlev_cycleDelay(20);
                    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
                    // ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_I);
                    // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_H, _statusInfo._debugIORQEvs[_statusInfo._debugIORQMatchNum]._addr);
                    // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_G, _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._addr);
                    // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_F, _statusInfo._debugIORQEvs[_statusInfo._debugIORQMatchNum]._flags);
                    // ISR_VALUE(ISR_ASSERT_CODE_DEBUG_E, _statusInfo._debugIORQEvsMatch[_statusInfo._debugIORQMatchNum]._flags);
                }

                _statusInfo._debugIORQMatchNum++;
            }
        }
    }
#endif

    // If Z80 is reading from the data bus (inc reading an ISR vector)
    // and result is valid then put the returned data onto the bus
    bool isReading = ((((ctrlBusVals & BR_CTRL_BUS_RD_MASK) != 0) && (((ctrlBusVals & BR_CTRL_BUS_MREQ_MASK) != 0) || ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) != 0))) ||
                     (((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) != 0) && ((ctrlBusVals & BR_CTRL_BUS_M1_MASK) != 0)));
    if (isReading && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        // TODO
        // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        // lowlev_cycleDelay(20);
        // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
        // Now driving data onto the target data bus


        // TODO
        // Need to make sure in all cases that OE FF is active
        // Currently this looks like it might not be the case
        // if _waitSuspendBusDetailOneCycle
        if (_waitSuspendBusDetailOneCycle)
            muxDataBusOutputEnable();


        // Set data direction out on the data bus driver
        write32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);
        pibSetOut();
        pibSetValue(retVal & 0xff);

#ifdef SET_DB_OUT

        // // TODO
        // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        // lowlev_cycleDelay(20);
        // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);

        // A flip-flop handles data OE during the IORQ/MREQ cycle and 
        // deactivates at the end of that cycle - so prime this flip-flop here
        // Actually this is already done above during reading from the data bus
        // so isn't actually required
        // muxDataBusOutputEnable();
#endif
        // lowlev_cycleDelay(CYCLES_DELAY_FOR_TARGET_READ);
        _targetReadInProgress = true;

        // uint32_t readLoopStartUs = micros();
        // while(true)
        // {

        //     uint32_t busVals = read32(ARM_GPIO_GPLEV0);

        //     // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
        //     // lowlev_cycleDelay(20);
        //     // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);

        //     if ((busVals & BR_RD_BAR_MASK) != 0)
        //         break;

        //     if (isTimeout(micros(), readLoopStartUs, BR_MAX_TIME_IN_READ_LOOP_US))
        //     {
        //         _targetReadInProgress = true;
        //         break;
        //     }
        // }

    }

    // // Elapsed and count
    // uint32_t isrElapsedUs = micros() - isrStartUs;
    // _statusInfo.isrCount++;

    // // Stats
    // if (ctrlBusVals & BR_CTRL_BUS_MREQ_MASK)
    // {
    //     if (ctrlBusVals & BR_CTRL_BUS_RD_MASK)
    //         _statusInfo.isrMREQRD++;
    //     else if (ctrlBusVals & BR_CTRL_BUS_WR_MASK)
    //         _statusInfo.isrMREQWR++;
    // }
    // else if (ctrlBusVals & BR_CTRL_BUS_IORQ_MASK)
    // {
    //     if (ctrlBusVals & BR_CTRL_BUS_RD_MASK)
    //         _statusInfo.isrIORQRD++;
    //     else if (ctrlBusVals & BR_CTRL_BUS_WR_MASK)
    //         _statusInfo.isrIORQWR++;
    //     else if (ctrlBusVals & BR_CTRL_BUS_M1_MASK)
    //         _statusInfo.isrIRQACK++;
    // }

    // // Overflows
    // if (_statusInfo.isrAccumUs > 1000000000)
    // {
    //     _statusInfo.isrAccumUs = 0;
    //     _statusInfo.isrAvgingCount = 0;
    // }

    // // Averages
    // if (isrElapsedUs < 1000000)
    // {
    //     _statusInfo.isrAccumUs += isrElapsedUs;
    //     _statusInfo.isrAvgingCount++;
    //     _statusInfo.isrAvgNs = _statusInfo.isrAccumUs * 1000 / _statusInfo.isrAvgingCount;
    // }

    // // Max
    // if (_statusInfo.isrMaxUs < isrElapsedUs)
    //     _statusInfo.isrMaxUs = isrElapsedUs;    
}