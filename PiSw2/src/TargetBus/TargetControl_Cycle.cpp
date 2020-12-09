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
    _cycleReqCompleteCB = NULL;
    _cycleReqPObject = NULL;
    _cycleReqSocketIdx = 0;
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

bool TargetControl::cycleReqAction(BR_BUS_ACTION busActionType,
            BR_BUS_ACTION_REASON busActionReason,
            uint32_t socketIdx,
            uint32_t maxDurationUs,
            BusCompleteCBFnType* cycleCompleteCB, 
            void* pObject)
{
    if (_programmingPending || (_cycleReqState != CYCLE_REQ_STATE_NONE))
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "cycleReqAction FAILED socketIdx %d state %d progPend %d",
        //         socketIdx, _cycleReqState, _programmingPending);
        return false;
    }
    _cycleReqCompleteCB = cycleCompleteCB;
    _cycleReqPObject = pObject;
    _cycleReqSocketIdx = socketIdx;
    _cycleReqState = CYCLE_REQ_STATE_PENDING;
    _cycleReqActionType = busActionType;
    _cycleBusRqReason = busActionReason;
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

void TargetControl::cycleService(bool serviceWaitOnly)
{
    // Check if suspended
    if (_isSuspended)
        return;

    // Check for WAIT and handle if asserted
    cycleCheckWait();

    // Check if actions should be serviced
    if (serviceWaitOnly)
        return;

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
        else
            cycleReqAssertedOther(_cycleReqActionType);
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

void TargetControl::cycleReqAssertedOther(BR_BUS_ACTION actionType)
{
    // Check for action complete - based on time
    if (isTimeout(micros(), _cycleReqAssertedUs, _cycleReqMaxUs))
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, 
        //          "Timeout on bus action %u type %d _waitIsActive %d", 
        //          micros(), _cycleReqActionType, _waitIsActive);
        cycleReqCallback(BR_OK);

        // Restore settings for fast-wait processing - this also clears all
        // actions since it uses the MUX and RESET, IRQ and NMI are generated
        // by the mux
        cycleSetupForFastWait();

    }
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
        _cycleReqCompleteCB(_cycleReqPObject, _cycleReqActionType, _cycleBusRqReason, _cycleReqSocketIdx, result);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle check for new wait and handle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleCheckWait()
{
    // Check if we need to complete a WAIT cycle
    // This happens when in debugger break
    if (_cycleHeldInWaitState)
    {
        cycleHandleHeldInWait();
        return;
    }

    // Loop here to decrease the time to handle a WAIT
    static const uint32_t WAIT_LOOPS_FOR_TIME_OPT = 20;
    uint32_t rawBusVals = 0;
    for (uint32_t waitLoopIdx = 0; waitLoopIdx < WAIT_LOOPS_FOR_TIME_OPT; waitLoopIdx++)
    {
        // Get raw GPIO pin values
        rawBusVals = read32(ARM_GPIO_GPLEV0);

        // Check control bus is stable
        if (!CTRL_BUS_IS_WAIT(rawBusVals))
            continue;

        // Check if we need to do full wait processing
        bool fullWaitProc = true;
        if (!(rawBusVals & BR_MREQ_BAR_MASK))
        {
            // Check mux is enabled for high-addr and mux is enabled
            if ((rawBusVals & (BR_MUX_CTRL_BIT_MASK | BR_MUX_EN_BAR_MASK)) == BR_MUX_HADDR_OE_MASK)
            {
                // Get high address
                uint32_t highAddr = (rawBusVals >> BR_DATA_BUS) & 0xff;

                // Check if in watch table
                if (_memWaitHighAddrWatch[highAddr] == 0)
                {
                    fullWaitProc = false;
                }
            }
        }

        // Full wait processing
        if (fullWaitProc || (_debuggerState != DEBUGGER_STATE_FREE_RUNNING))
        {
            // Full wait processing - clears wait flip-flops as needed
            cycleFullWaitProcessing(rawBusVals);
        }
        else
        {
            // Clear the wait
            BusRawAccess::waitResetFlipFlops();
        }
        break;
    }

    // TODO 2020 implement access stats??
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

void TargetControl::cycleFullWaitProcessing(uint32_t rawBusVals)
{
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
    bool debugHold = debuggerHandleWaitCycle(addr, dataBusVals, rawBusVals);

    // Callback to sockets
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    if (_pBusAccessCB)
    {
        uint32_t ctrlBusVals = 
                (((rawBusVals & BR_RD_BAR_MASK) == 0) ? BR_CTRL_BUS_RD_MASK : 0) |
                (((rawBusVals & BR_WR_BAR_MASK) == 0) ? BR_CTRL_BUS_WR_MASK : 0) |
                (((rawBusVals & BR_MREQ_BAR_MASK) == 0) ? BR_CTRL_BUS_MREQ_MASK : 0) |
                (((rawBusVals & BR_IORQ_BAR_MASK) == 0) ? BR_CTRL_BUS_IORQ_MASK : 0) |
                (((rawBusVals & BR_WAIT_BAR_MASK) == 0) ? BR_CTRL_BUS_WAIT_MASK : 0) |
                (((rawBusVals & BR_V20_M1_BAR_MASK) == 0) ? BR_CTRL_BUS_M1_MASK : 0) |
                (((rawBusVals & BR_BUSACK_BAR_MASK) == 0) ? BR_CTRL_BUS_BUSACK_MASK : 0);
        _pBusAccessCB(_pBusAccessCBObject, addr, dataBusVals, ctrlBusVals, retVal);
    }

#ifdef DEBUG_SHOW_IO_ACCESS_ADDRS_RD
    if ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) && (ctrlBusVals & BR_CTRL_BUS_RD_MASK))
        DEBUG_VAL_SET(6, retVal);
#endif

    // Handle processor read (means we have to push data onto the data bus)
    if (CTRL_BUS_IS_READ(rawBusVals) && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        // Set data direction out on the data bus driver
        write32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);
        _busControl.bus().pibSetOut();

        // Set the value for the processor to read
        _busControl.bus().pibSetValue(retVal & 0xff);

        // Check if we are going to hold here - if so return without
        // releasing the WAIT
        _cycleWaitForReadCompletionRequired = true;
        if (debugHold)
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
        if (debugHold)
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
        uint32_t rawBusVals = read32(ARM_GPIO_GPLEV0);

        // Check if a neither IORQ or MREQ asserted
        if (((rawBusVals & BR_MREQ_BAR_MASK) != 0) && ((rawBusVals & BR_IORQ_BAR_MASK) != 0))
            return true;
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
