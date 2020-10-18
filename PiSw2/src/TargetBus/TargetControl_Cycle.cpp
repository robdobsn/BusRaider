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
            cycleReqCompleted(BR_NOT_HANDLED);
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
            cycleReqCompleted(BR_OK);
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
            cycleReqCompleted(BR_NOT_HANDLED);
            _busAccess.bus().setBusSignal(BR_BUS_ACTION_BUSRQ, false);
        }
    }
}

void TargetControl::cycleReqAssertedIRQ()
{

}

void TargetControl::cycleReqAssertedOther()
{

}

void TargetControl::cycleReqCompleted(BR_RETURN_TYPE result)
{
    _cycleReqState = CYCLE_REQ_STATE_NONE;
    if (_cycleReqCompleteCB)
    {
        _cycleReqCompleteCB(_cycleReqPObject, _cycleReqSlotIdx, result);
    }
}
