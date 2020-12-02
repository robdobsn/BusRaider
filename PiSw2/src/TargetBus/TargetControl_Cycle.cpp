// Bus Raider
// Rob Dobson 2019

#include "TargetControl.h"
#include "BusControl.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include "DebugHelper.h"

// #define TEST_HIGH_ADDR_IS_ON_PIB
// #define DEBUG_SHOW_IO_ACCESS_ADDRS_WR
// #define DEBUG_SHOW_IO_ACCESS_ADDRS_RD

// Module name
static const char MODULE_PREFIX[] = "TargCtrlCyc";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle clear
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleClear()
{
    _cycleReqInfo = NULL;
    _cycleReqCompleteCB = NULL;
    _cycleReqPObject = NULL;
    _cycleReqSlotIdx = 0;
    _cycleReqState = CYCLE_REQ_STATE_NONE;
    _cycleReqActionType = BR_BUS_ACTION_NONE;
    _cycleBusRqReason = BR_BUS_ACTION_GENERAL;
    _cycleReqUs = 0;
    _cycleReqAssertedUs = 0;
    _cycleReqMaxUs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Suspend cycle processing
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleSuspend(bool suspend)
{
    // Handle restoration
    if (!suspend)
    {
        // Setup for fast wait processing
        cycleSetupForFastWait();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Req cycle action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetControl::cycleReqAction(BusSocketInfo& busSocketInfo, 
            uint32_t maxDurationUs,
            BusCompleteCBFnType* cycleCompleteCB, 
            void* pObject,
            uint32_t slotIdx)
{
    if (_programmingPending || (_cycleReqState != CYCLE_REQ_STATE_NONE))
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "cycleReqAction FAILED state %d progPend %d",
        //         _cycleReqState, _programmingPending);
        return false;
    }
    _cycleReqInfo = &busSocketInfo;
    _cycleReqCompleteCB = cycleCompleteCB;
    _cycleReqPObject = pObject;
    _cycleReqSlotIdx = slotIdx;
    _cycleReqState = CYCLE_REQ_STATE_PENDING;
    _cycleReqActionType = busSocketInfo.getType();
    _cycleBusRqReason = busSocketInfo.busMasterReason;
    _cycleReqUs = micros();
    _cycleReqAssertedUs = 0;
    _cycleReqMaxUs = maxDurationUs; 
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "cycleReqAction type %d reason %d progPending %d",
    //         _cycleReqActionType, _cycleBusRqReason, _programmingPending);
    return true;
}

void TargetControl::cycleClearAction()
{
    _cycleReqState = CYCLE_REQ_STATE_NONE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleService()
{
    // Check if suspended
    if (_isSuspended)
        return;

    // Check for WAIT and handle if asserted
    cycleCheckWait();

    // Handle cycle actions
    cycleHandleActions();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle pending helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleHandleActions()
{
    // Check for programming required
    if (_programmingPending && (_cycleReqState == CYCLE_REQ_STATE_NONE))
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "cycleService Programming BUSRQ");
        // Programming
        _cycleReqActionType = BR_BUS_ACTION_BUSRQ;
        _cycleBusRqReason = BR_BUS_ACTION_PROGRAMMING;
        _cycleReqMaxUs = PROG_MAX_WAIT_FOR_BUSAK_US;
        _cycleReqState = CYCLE_REQ_STATE_PENDING;
    }

    // Handle cycle state
    if (_cycleReqState == CYCLE_REQ_STATE_PENDING)
    {
        // Deal with debugger
        if (_debuggerState != DEBUGGER_STATE_FREE_RUNNING)
        {
            // TODO 2020 deal with IRQ, etc

            // Handle the request as though it had been performed
            // If this was a display refresh then the memory access
            // will come from a cache. Other actions like programming,
            // IRQ generation, etc will also be handled appropriately
            // by the debugger code
            cyclePerformActionRequest();
        }
        else
        {
            // Bus actions pending
            cycleReqHandlePending();
        }
    }
    else if (_cycleReqState == CYCLE_REQ_STATE_ASSERTED)
    {
        // Bus actions asserted
        if (_cycleReqActionType == BR_BUS_ACTION_BUSRQ)
            cycleReqAssertedBusRq();
        else if (_cycleReqActionType == BR_BUS_ACTION_IRQ)
            cycleReqAssertedIRQ();
        else
            cycleReqAssertedOther();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle pending helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleReqHandlePending()
{
    // Initiate the cycle action
    _busControl.bus().setBusSignal(_cycleReqActionType, true);
    _cycleReqAssertedUs = micros();
    _cycleReqState = CYCLE_REQ_STATE_ASSERTED;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle asserted helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleReqAssertedBusRq()
{
    // Check for BUSACK
    if (_busControl.bus().rawBUSAKActive())
    {
        // Take control of bus
        _busControl.bus().busReqTakeControl();

        // Check reason for request
        cyclePerformActionRequest();

        // Release bus
        _busControl.bus().busReqRelease();
    }
    else
    {
        // Check for timeout on BUSACK
        if (isTimeout(micros(), _cycleReqAssertedUs, _cycleReqMaxUs))
        {
            // For bus requests a timeout means failure
            cycleReqCallback(BR_NOT_HANDLED);
            _busControl.bus().setBusSignal(BR_BUS_ACTION_BUSRQ, false);
        }
    }

    // Restore settings for fast-wait processing
    cycleSetupForFastWait();
}

void TargetControl::cycleReqAssertedIRQ()
{
    // TODO 2020 - handle IRQ
    
    // Restore settings for fast-wait processing
    cycleSetupForFastWait();
}

void TargetControl::cycleReqAssertedOther()
{
    // TODO 2020 - handle other

    // Restore settings for fast-wait processing
    cycleSetupForFastWait();
}

void TargetControl::cyclePerformActionRequest()
{
    if (_cycleBusRqReason == BR_BUS_ACTION_PROGRAMMING)
    {
        programmingWrite();
        programmingDone();
        _cycleReqState = CYCLE_REQ_STATE_NONE;
    }
    else
    {
        // Callback on completion now so that any new action raised by the callback
        // such as a reset, etc can be asserted before BUSRQ is released
        cycleReqCallback(BR_OK);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle callback on action request
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleReqCallback(BR_RETURN_TYPE result)
{
    // Perform cycle callback
    _cycleReqState = CYCLE_REQ_STATE_NONE;
    if (_cycleReqCompleteCB)
    {
        _cycleReqCompleteCB(_cycleReqPObject, _cycleReqSlotIdx, result);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle check for new wait and handle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleCheckWait()
{
    // Check suspended
    if (_isSuspended)
        return;

    // Check if we need to complete a WAIT cycle
    // This happens when in debugger break
    if (_cycleHeldInWaitState)
    {
        cycleHandleHeldInWait();
        return;
    }

    // Loop here to decrease the time to handle a WAIT
    static const uint32_t WAIT_LOOPS_FOR_TIME_OPT = 20;
    uint32_t busVals = 0;
    for (uint32_t waitLoopIdx = 0; waitLoopIdx < WAIT_LOOPS_FOR_TIME_OPT; waitLoopIdx++)
    {
        // Get inputs
        busVals = read32(ARM_GPIO_GPLEV0);

        // Check for WAIT active (and BUSACK not active) - otherwise nothing to do
        if ((busVals & BR_WAIT_BAR_MASK) || (!(busVals & BR_BUSACK_BAR_MASK)))
            continue;

        // Check RD or WR is active - nothing to do if not
        if ((busVals & BR_RD_BAR_MASK) && (busVals & BR_WR_BAR_MASK))
            continue;

        // Check if we need to do full wait processing
        bool fullWaitProc = true;
        if (!(busVals & BR_MREQ_BAR_MASK))
        {
            // Get high address
            uint32_t highAddr = (busVals >> BR_DATA_BUS) & 0xff;

            // Check if in watch table
            if (_memWaitHighAddrWatch[highAddr] == 0)
                fullWaitProc = false;
        }

        // Full wait processing
        if (fullWaitProc || (_debuggerState != DEBUGGER_STATE_FREE_RUNNING))
        {
            // Full wait processing - clears wait flip-flops as needed
            cycleFullWaitProcessing();
        }
        else
        {
            // Clear the wait
            BusRawAccess::waitResetFlipFlops();
        }
        break;
    }
// #else
//     // TODO 2020 - assumes simple mode - no debug:
//     // - processor will not be held in wait
//     // - processor read from emulated IO or MEM is handled completely
//     //   in this function - right to the end of the cycle

//     // Loop here to reduce time to detect WAIT
//     static const uint32_t WAIT_LOOPS_FOR_TIME_OPT = 4;
//     bool waitFound = false;
//     uint32_t busVals = 0;
//     for (uint32_t waitLoopIdx = 0; waitLoopIdx < WAIT_LOOPS_FOR_TIME_OPT; waitLoopIdx++)
//     {
//         // Get inputs
//         busVals = read32(ARM_GPIO_GPLEV0);

//         // Check for WAIT active (and BUSACK not active) - otherwise nothing to do
//         if ((busVals & BR_WAIT_BAR_MASK) || (!(busVals & BR_BUSACK_BAR_MASK)))
//             continue;

//         // Check RD or WR is active - nothing to do if not
//         if ((busVals & BR_RD_BAR_MASK) && (busVals & BR_WR_BAR_MASK))
//             continue;
        
//         // Found a WAIT
//         waitFound = true;
//         break;
//     }
//     if (!waitFound)
//         return;

//     // Check for IORQ
//     if (!(busVals & BR_IORQ_BAR_MASK))
//     {
//         cycleFullWaitProcessing();
//     }
//     else if (!(busVals & BR_MREQ_BAR_MASK))
//     {
//         // Get high address
//         uint32_t highAddr = (busVals >> BR_DATA_BUS) & 0xff;

//         // Check if in watch table
//         if (_memWaitHighAddrWatch[highAddr] != 0)
//             cycleFullWaitProcessing();
//     }

//     // High address stats
//     // _memWaitHighAddrLookup[highAddr]++;
//     // uint32_t foundVals = 0;
//     // for (uint32_t i = 0; i < MEM_WAIT_HIGH_ADDR_LOOKUP_LEN; i++)
//     // {
//     //     if (_memWaitHighAddrLookup[i] > 0)
//     //     {
//     //         if (foundVals < 5)
//     //             DEBUG_VAL_SET(foundVals+1, i);
//     //         foundVals++;
//     //     }
//     //     DEBUG_VAL_SET(0, foundVals);
//     // }

//     // Debug
//     // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
//     // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
//     // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
//     // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
//     // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);

//     // TODO 2020
//     _busControl.bus().waitResetFlipFlops();
// #endif
//     // Check if already in wait
//     if (!_waitIsActive)
//     {
//         // Check new wait type
//         uint32_t busVals = read32(ARM_GPIO_GPLEV0);

//         // Ignore if there is no WAIT or we're in BUSAK
//         if (((busVals & BR_WAIT_BAR_MASK) == 0) && ((busVals & BR_BUSACK_BAR_MASK) != 0))
//         {
//             // Handle the wait if there isn't an IRQ/NMI/RESET action in progress
//             // - because these actions use the MUX and we can't handle the wait
//             // properly when they are in progress
//             _waitIsActive = (_cycleReqState != CYCLE_REQ_STATE_ASSERTED) ||
//                    (_cycleReqActionType == BR_BUS_ACTION_BUSRQ);
//         }

//         // Handle wait newly asserted
//         if (_waitIsActive)
//         {
//             // Check for MREQ
//             if ((busVals & BR_MREQ_BAR_MASK) == 0)
//             {
//                 // This may be a refesh cycle (logic to skip refresh cycles doesn't
//                 // always work - so may need to wait until RD or WR become active
//                 uint32_t curUs = micros();
//                 while ((busVals & BR_RD_BAR_MASK) && (busVals & BR_RD_BAR_MASK))
//                 {
//                     if (isTimeout(micros(), curUs, MAX_WAIT_FOR_CTRL_BUS_VALID_US))
//                         break;
//                 }
//                 // The high address should be on the PIB at this time
//                 uint32_t highAddr = (busVals >> BR_DATA_BUS) & 0xff;
// #ifdef TEST_HIGH_ADDR_IS_ON_PIB
//                 digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
//                 microsDelay(1);
//                 digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
//                 cycleSetupForFastWait();
//                 lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_READ);
//                 uint32_t testAddr = (read32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
//                 if (highAddr != testAddr)
//                     DEBUG_VAL_INC(0);
// #endif
//                 // TODO 2020
//                 // _memWaitHighAddrLookup
//                 cycleFullWaitProcessing();
//             }
//             // Check for IORQ and not M1
//             else if (((busVals & BR_IORQ_BAR_MASK) == 0) && (busVals & BR_V20_M1_BAR_MASK) != 0)
//             {
//                 cycleFullWaitProcessing();
//             }

//             // TODO 2020 - handle INT_ACK and MREQ
//         }
//     }

//     // Handle existing wait conditions - including ones just started above
//     // TODO - handle hold in wait
//     if (_waitIsActive)
//     {
//         // Release wait
//         _busControl.bus().waitResetFlipFlops();
//         _waitIsActive = false;

//         // Handle read release if required
//         cycleHandleReadRelease();
//     }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup for fast wait
// Routes the high-address bus to PIB (so it can be read immediately)
// Also used at end of processor read cycle as the same thing is required
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleSetupForFastWait()
{
    // Set data bus direction in
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);
    // Ensure PIB is inward
    _busControl.bus().pibSetIn();
    // high address is enabled on mux
    _busControl.bus().muxSet(BR_MUX_HADDR_OE_BAR);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle handle important wait
// Called when any IORQ wait is detected
// Called for an MREQ wait that matches a high-addr of interest
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleFullWaitProcessing()
{
    // Read control lines
    uint32_t ctrlBusVals = _busControl.bus().controlBusRead();

    // Read address and data
    uint32_t addr = 0;
    uint32_t dataBusVals = 0;
    _busControl.bus().addrAndDataBusRead(addr, dataBusVals);

#ifdef DEBUG_SHOW_IO_ACCESS_ADDRS_WR
    if ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) && (ctrlBusVals & BR_CTRL_BUS_WR_MASK))
    {
        DEBUG_VAL_SET(0, addr);
        DEBUG_VAL_SET(1, dataBusVals);
        DEBUG_VAL_SET(2, ctrlBusVals);
    }
#endif
#ifdef DEBUG_SHOW_IO_ACCESS_ADDRS_RD
    if ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) && (ctrlBusVals & BR_CTRL_BUS_RD_MASK))
    {
        DEBUG_VAL_SET(3, addr);
        DEBUG_VAL_SET(4, dataBusVals);
        DEBUG_VAL_SET(5, ctrlBusVals);
    }
#endif

    // Handle debugger access to registers and memory
    debuggerGrabRegsAndMemory(ctrlBusVals, addr, dataBusVals);

    // Callback to sockets
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    if (_pBusAccessCB)
    {
        _pBusAccessCB(_pBusAccessCBObject, addr, dataBusVals, ctrlBusVals, retVal);
    }

#ifdef DEBUG_SHOW_IO_ACCESS_ADDRS_RD
    if ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) && (ctrlBusVals & BR_CTRL_BUS_RD_MASK))
        DEBUG_VAL_SET(6, retVal);
#endif

    // Handle processor read (means we have to push data onto the data bus)
    if (CTRL_BUS_IS_READ(ctrlBusVals) && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        // Set data direction out on the data bus driver
        write32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);
        _busControl.bus().pibSetOut();

        // Set the value for the processor to read
        _busControl.bus().pibSetValue(retVal & 0xff);

        // Check if we are going to hold here - if so return without
        // releasing the WAIT
        _cycleWaitForReadCompletionRequired = true;
        if (_debuggerState != DEBUGGER_STATE_FREE_RUNNING)
        {
            _cycleHeldInWaitState = true;
            return;
        }

        // Clear the wait
        BusRawAccess::waitResetFlipFlops();
        
        // Stay here until read cycle is complete
        cycleWaitForReadCompletion();
    }
    else
    {
        // Check if we are going to hold here - if so return without
        // releasing the WAIT
        if (_debuggerState != DEBUGGER_STATE_FREE_RUNNING)
        {
            _cycleHeldInWaitState = true;
            return;
        }

        // Clear the wait
        BusRawAccess::waitResetFlipFlops();    
    }

    // Clear output and setup for fast wait
    cycleSetupForFastWait();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait for completion of a read operation
// This is needed to ensure the data bus is returned to input mode once a read is completed
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TargetControl::cycleWaitForReadCompletion()
{
    _cycleWaitForReadCompletionRequired = false;
    uint32_t waitForReadCompleteStartUs = micros();
    while(!isTimeout(micros(), waitForReadCompleteStartUs, MAX_WAIT_FOR_END_OF_READ_US))
    {
        // Read the control lines
        uint32_t busVals = read32(ARM_GPIO_GPLEV0);

        // Check if a neither IORQ or MREQ asserted
        if (((busVals & BR_MREQ_BAR_MASK) != 0) && ((busVals & BR_IORQ_BAR_MASK) != 0))
        {
            // TODO 2020
            // // Check if paging in/out is required
            // if (_targetPageInOnReadComplete)
            // {
            //     busAccessCallbackPageIn();
            //     _targetPageInOnReadComplete = false;
            // }

            return true;
        }
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle held in WAIT
// This happens when a cycle is held in WAIT because of debugger
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleHandleHeldInWait()
{
    // Check if debugging has released (even for one step)
    if ((_debuggerState == DEBUGGER_STATE_FREE_RUNNING) || 
        (_debuggerStepMode == DEBUGGER_STEP_MODE_STEP_INTO))
    {
        _debuggerStepMode = DEBUGGER_STEP_MODE_NONE;
        _cycleHeldInWaitState = false;

        // Clear the wait
        BusRawAccess::waitResetFlipFlops();

        // Check if we need to wait until read cycle is completed
        if (_cycleWaitForReadCompletionRequired)
        {
            // Stay here until read cycle is complete
            cycleWaitForReadCompletion();
        }

        // Clear output and setup for fast wait
        cycleSetupForFastWait();        
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read release
// Stop outputting data onto data bus after processor read cycle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void TargetControl::cycleHandleReadRelease()
// {
//     if (!_cycleReadInProgress)
//         return;

//     // Stay here until read cycle is complete
//     uint32_t waitForReadCompleteStartUs = micros();
//     while(!isTimeout(micros(), waitForReadCompleteStartUs, MAX_WAIT_FOR_END_OF_READ_US))
//     {
//         // Read the control lines
//         uint32_t busVals = read32(ARM_GPIO_GPLEV0);

//         // Check if a neither IORQ or MREQ asserted
//         if (((busVals & BR_MREQ_BAR_MASK) != 0) && ((busVals & BR_IORQ_BAR_MASK) != 0))
//         {
//             // TODO 2020
//             // // Check if paging in/out is required
//             // if (_targetPageInOnReadComplete)
//             // {
//             //     busAccessCallbackPageIn();
//             //     _targetPageInOnReadComplete = false;
//             // }

//             break;
//         }
//     }

//     // Clear output and setup for fast wait
//     cycleSetupForFastWait();
//     _cycleReadInProgress = false;
// }
