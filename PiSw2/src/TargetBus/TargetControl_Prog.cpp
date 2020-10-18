// Bus Raider
// Rob Dobson 2019

#include "TargetControl.h"
#include "BusControl.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"

// Module name
static const char MODULE_PREFIX[] = "TargCtrlProg";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target programming
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TargetControl::programmingStart(bool execAfterProgramming)
{
    // TODO 2020
    // // Check there is something to write
    // if (targetProgrammer.numMemoryBlocks() == 0) 
    // {
    //     // Nothing new to write
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingStart - nothing to write");
    // } 
    // else 
    // {
    //     // BUSRQ is used even if memory is emulated because it holds the processor while changes are made
    //     // Give the BusAccess some service first to ensure WAIT handling is complete before requesting the bus
    //     for (int i = 0; i < 3; i++)
    //         _busAccess.service();

    //     // Request target bus
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingStart targetReqBus");
    //     _busAccess.socketReqBus(_busSocketId, BR_BUS_ACTION_PROGRAMMING);
    //     _busActionPendingProgramTarget = true;
    //     _busActionPendingExecAfterProgram = execAfterProgramming;
    // }
}

void TargetControl::programmingDone()
{
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "programmingDone");

    // TODO 2020
    // Set flag to indicate mode
    // _stepMode = STEP_MODE_STEP_PAUSED;

    // // Release bus hold if held
    // if (_busAccess.waitIsHeld())
    // {
    //     // Remove any hold to allow execution / injection
    //     _busAccess.waitHold(_busSocketId, false);
    // }
}