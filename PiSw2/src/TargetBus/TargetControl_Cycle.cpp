// Bus Raider
// Rob Dobson 2019

#include "TargetControl.h"
#include "BusControl.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"

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
    _cycleReadInProgress = false;
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
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "cycleReqAction FAILED state %d progPend %d",
                _cycleReqState, _programmingPending);
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

    // TODO 2020 - optimize
    cycleNewWait();

    // Handle cycle actions
    if (_programmingPending && (_cycleReqState == CYCLE_REQ_STATE_NONE))
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "cycleService Programming BUSRQ");
        // Programming
        _cycleReqActionType = BR_BUS_ACTION_BUSRQ;
        _cycleBusRqReason = BR_BUS_ACTION_PROGRAMMING;
        _cycleReqMaxUs = PROG_MAX_WAIT_FOR_BUSAK_US;
        cycleReqHandlePending(true);
    }
    else if (_cycleReqState == CYCLE_REQ_STATE_PENDING)
    {
        // Bus actions pending
        cycleReqHandlePending(false);
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

void TargetControl::cycleReqHandlePending(bool isProgramming)
{
    // TODO 2020 - check if we can assert the bus action - maybe need more conditions
    if (true) // !_busAccess.bus().waitIsActive())
    {
        // Initiate the cycle action
        _busAccess.bus().setBusSignal(_cycleReqActionType, true);
        _cycleReqAssertedUs = micros();
        _cycleReqState = CYCLE_REQ_STATE_ASSERTED;
    }
    else if (!isProgramming)
    {
        // Check for timeout
        if (isTimeout(micros(), _cycleReqUs, MAX_WAIT_FOR_PENDING_ACTION_US))
        {
            // Cancel the request
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, 
            //          "Timeout on bus action %u type %d _waitIsActive %d", 
            //          micros(), _cycleReqActionType, _waitIsActive);
            cycleReqCallback(BR_NOT_HANDLED);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle asserted helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleReqAssertedBusRq()
{
    // Check for BUSACK
    if (_busAccess.bus().rawBUSAKActive())
    {
        // Take control of bus
        _busAccess.bus().busReqTakeControl();

        // Check reason for request
        if (_cycleBusRqReason == BR_BUS_ACTION_PROGRAMMING)
        {
            programmingWrite();
            programmingDone();
        }
        else
        {
            // Callback on completion now so that any new action raised by the callback
            // such as a reset, etc can be asserted before BUSRQ is released
            cycleReqCallback(BR_OK);
        }

        // Release bus
        _busAccess.bus().busReqRelease();
    }
    else
    {
        // Check for timeout on BUSACK
        if (isTimeout(micros(), _cycleReqAssertedUs, _cycleReqMaxUs))
        {
            // For bus requests a timeout means failure
            cycleReqCallback(BR_NOT_HANDLED);
            _busAccess.bus().setBusSignal(BR_BUS_ACTION_BUSRQ, false);
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
// Cycle handle new wait
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleNewWait()
{
    // Check suspended
    if (_isSuspended)
        return;

    // Check if already in wait
    if (!_waitIsActive)
    {
        // Check new wait type
        uint32_t busVals = read32(ARM_GPIO_GPLEV0);

        // Ignore if there is no WAIT or we're in BUSAK
        if (((busVals & BR_WAIT_BAR_MASK) == 0) && ((busVals & BR_BUSACK_BAR_MASK) != 0))
        {
            // Handle the wait if there isn't an IRQ/NMI/RESET action in progress
            // - because these actions use the MUX and we can't handle the wait
            // properly when they are in progress
            _waitIsActive = (_cycleReqState != CYCLE_REQ_STATE_ASSERTED) ||
                   (_cycleReqActionType == BR_BUS_ACTION_BUSRQ);
        }

        // Handle wait newly asserted
        if (_waitIsActive)
        {
            // Check for IORQ and not M1
            if (((busVals & BR_IORQ_BAR_MASK) == 0) && (busVals & BR_V20_M1_BAR_MASK) != 0)
            {
                cycleHandleIORQ();
            }

            // TODO 2020 - handle INT_ACK and MREQ
        }
    }

    // Handle existing wait conditions - including ones just started above
    // TODO - handle hold in wait
    if (_waitIsActive)
    {
        // Release wait
        _busAccess.bus().waitResetFlipFlops();
        _waitIsActive = false;

        // Handle read release if required
        cycleHandleReadRelease();
    }
}

void TargetControl::cycleSetupForFastWait()
{
    // Ensure PIB is inward
    _busAccess.bus().pibSetIn();
    // high address is enabled on mux
    _busAccess.bus().muxSet(BR_MUX_HADDR_OE_BAR);
    // Set data bus direction in
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);

}

void TargetControl::cycleHandleIORQ()
{
    // Read control lines
    uint32_t ctrlBusVals = _busAccess.bus().controlBusRead();

    // Read address and data
    uint32_t addr = 0;
    uint32_t dataBusVals = 0;
    _busAccess.bus().addrAndDataBusRead(addr, dataBusVals);

    // Callback to sockets
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
//     if (_pSocketBusCallback)
//         _pSocketBusCallback(addr, dataBusVals, ctrlBusVals, retVal);

    // Handle processor read (means we have to push data onto the data bus)
    if (CTRL_BUS_IS_READ(ctrlBusVals) && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        // Set data direction out on the data bus driver
        write32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);
        _busAccess.bus().pibSetOut();
        _busAccess.bus().pibSetValue(retVal & 0xff);
        _cycleReadInProgress = true;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read release
// Stop outputting data onto data bus after processor read cycle
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleHandleReadRelease()
{
    if (!_cycleReadInProgress)
        return;

    // Stay here until read cycle is complete
    uint32_t waitForReadCompleteStartUs = micros();
    while(!isTimeout(micros(), waitForReadCompleteStartUs, MAX_WAIT_FOR_END_OF_READ_US))
    {
        // Read the control lines
        uint32_t busVals = read32(ARM_GPIO_GPLEV0);

        // Check if a neither IORQ or MREQ asserted
        if (((busVals & BR_MREQ_BAR_MASK) != 0) && ((busVals & BR_IORQ_BAR_MASK) != 0))
        {
            // Clear output and setup for fast wait
            cycleSetupForFastWait();

            // TODO 2020
            // // Check if paging in/out is required
            // if (_targetPageInOnReadComplete)
            // {
            //     busAccessCallbackPageIn();
            //     _targetPageInOnReadComplete = false;
            // }

            break;
        }
    }
    _cycleReadInProgress = false;
}
