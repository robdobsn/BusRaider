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
    _cycleReqUs = 0;
    _cycleReqAssertedUs = 0;
    _cycleReqMaxUs = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Req cycle action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleReqAction(BusSocketInfo& busSocketInfo, 
            uint32_t maxDurationUs,
            BusCompleteCBFnType* cycleCompleteCB, 
            void* pObject,
            uint32_t slotIdx)
{
    _cycleReqInfo = &busSocketInfo;
    _cycleReqCompleteCB = cycleCompleteCB;
    _cycleReqPObject = pObject;
    _cycleReqSlotIdx = slotIdx;
    _cycleReqState = CYCLE_REQ_STATE_PENDING;
    _cycleReqActionType = busSocketInfo.getType();
    _cycleReqUs = micros();
    _cycleReqAssertedUs = 0;
    _cycleReqMaxUs = maxDurationUs; 
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
    // Check for cycle requests pending
    if (_cycleReqState == CYCLE_REQ_STATE_PENDING)
    {
        // TODO 2020 - check if we can assert the bus action - maybe need more conditions
        if (!_busAccess.bus().waitIsActive())
        {
            // Initiate the cycle action
            _busAccess.bus().setBusSignal(_cycleReqActionType, true);
            _cycleReqAssertedUs = micros();
            _cycleReqState = CYCLE_REQ_STATE_ASSERTED;
        }
        else
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
    else if (_cycleReqState == CYCLE_REQ_STATE_ASSERTED)
    {
        if (_cycleReqActionType == BR_BUS_ACTION_BUSRQ)
            cycleReqAssertedBusRq();
        else if (_cycleReqActionType == BR_BUS_ACTION_IRQ)
            cycleReqAssertedIRQ();
        else
            cycleReqAssertedOther();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::cycleReqAssertedBusRq()
{
    // Check for BUSACK
    if (_busAccess.bus().rawBUSAKActive())
    {
        // Take control of bus
        _busAccess.bus().busReqTakeControl();

        // Callback on completion now so that any new action raised by the callback
        // such as a reset, etc can be asserted before BUSRQ is released
        cycleReqCompleted(BR_OK);

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
