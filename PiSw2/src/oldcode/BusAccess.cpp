// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "PiWiring.h"
#include <circle/interrupt.h>
#include "lowlib.h"
#include <circle/bcm2835.h>
#include "logging.h"
#include "circle/timer.h"

#define SERVICE_PERFORMANCE_OPTIMIZED 1

// Uncomment to drive wait states through timer - alternatively use service()
// #define TIMER_BASED_WAIT_STATES 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char MODULE_PREFIX[] = "BusAccess";

// Constructor

BusAccess::BusAccess()
    : _targetController(*this)
{
    // Hardware version
    _hwVersionNumber = HW_VERSION_DEFAULT;

    // Bus sockets
    _busSocketCount = 0;

    // Bus service enabled - can be disabled to allow external API to completely control bus
    _busServiceEnabled = true;

    // Wait state enables
    _waitOnMemory = false;
    _waitOnIO = false;

    // Wait is asserted (processor held)
    _waitAsserted = false;

    // Rate at which wait is released when free-cycling
    _waitCycleLengthUs = 1;
    _waitAssertedStartUs = 0;

    // Held in wait state
    _waitHold = false;

    // Wait suspend bus detail for one cycle
    _waitSuspendBusDetailOneCycle = false;

    // Bus action handling
    _busActionSocket = 0;
    _busActionType = BR_BUS_ACTION_NONE;
    _busActionInProgressStartUs = 0;
    _busActionAssertedStartUs = 0;
    _busActionAssertedMaxUs = 0;
    _busActionState = BUS_ACTION_STATE_NONE;
    _busActionSyncWithWait = false;

    // Paging
    _targetPageInOnReadComplete = false;

    // Target read in progress
    _targetReadInProgress = false;

    // Bus under BusRaider control
    _busReqAcknowledged = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise the bus raider bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::init()
{
    // Clock
    clockSetup();
    clockSetFreqHz(1000000);
    clockEnable(true);
    
    // Pins that are not setup here as outputs will be inputs
    // This includes the "bus" used for address-low/high and data (referred to as the PIB)

    // Setup the multiplexer
    setPinOut(BR_MUX_0, 0);
    setPinOut(BR_MUX_1, 0);
    setPinOut(BR_MUX_2, 0);

    // Set the MUX enable on V2.0 hardware - on V1.7 this is LADDR_CLK but it is an
    // output either way which is good as this code is only run once so can't take
    // into account the hardware version which comes from the ESP32 later on
    setPinOut(BR_MUX_EN_BAR, 1);
    
    // Bus Request
    setPinOut(BR_BUSRQ_BAR, 1);
    _busReqAcknowledged = false;
        
    // High address clock
    setPinOut(BR_HADDR_CK, 0);
    
    // Data bus driver direction inward
    setPinOut(BR_DATA_DIR_IN, 1);

    // Paging initially inactive
    setPinOut(BR_PAGING_RAM_PIN, 1);
    busPagePinSetActive(false);

    // Setup MREQ and IORQ enables
    waitSystemInit();
    _waitAsserted = false;

    // Remove edge detection
#ifdef ISR_TEST
    write32(ARM_GPIO_GPREN0, 0);
    write32(ARM_GPIO_GPAREN0, 0);
    write32(ARM_GPIO_GPFEN0, 0);
    write32(ARM_GPIO_GPAFEN0, 0); //read32(ARM_GPIO_GPFEN0) & (~(1 << BR_WAIT_BAR)));

    // // Clear any currently detected edge
    write32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);

    CInterrupts::connectFIQ(52, stepTimerISR, 0);
#endif

    // Heartbeat timer
#ifdef TIMER_BASED_WAIT_STATES
    Timers::set(TIMER_ISR_PERIOD_US, BusAccess::stepTimerISR, NULL);
    Timers::start();
#endif

    // Init the Target control
    _targetControl.init();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Re-initialise the bus raider bus
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::busAccessReinit()
{
    // Instruct extension hardware to page in
    busAccessCallbackPageIn();

    // Clear mux
    muxClear();

    // Set data direction inwards
    pibSetIn();
    write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);
    _targetReadInProgress = false;

    // Wait cycle length
    _waitCycleLengthUs = 0;

    // Check for bus ack released
    waitForBusAck(false);

    // Disable wait
    waitGenerationDisable();

    // Clear any wait condition
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "busAccessReinit");
    waitResetFlipFlops(true);

    // Update wait state generation
    // Debug
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "busAccessReinit");
    waitEnablementUpdate();

    // Reactivate bus service
    _busServiceEnabled = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Host Actions (Reset/NMI/IRQ)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reset the host
void BusAccess::targetReqReset(int busSocket, int durationTStates)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].resetDurationTStates = (durationTStates <= 0) ? BR_RESET_PULSE_T_STATES : durationTStates;
    _busSockets[busSocket].resetPending = true;
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetReqReset");
}

// Non-maskable interrupt the host
void BusAccess::targetReqNMI(int busSocket, int durationTStates)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    // Request NMI
    _busSockets[busSocket].nmiDurationTStates = (durationTStates <= 0) ? BR_NMI_PULSE_T_STATES : durationTStates;
    _busSockets[busSocket].nmiPending = true;
}

// Maskable interrupt the host
void BusAccess::targetReqIRQ(int busSocket, int durationTStates)
{
    // Check validity
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "ReqIRQ sock %d us %d", busSocket, _busSockets[busSocket].busActionDurationUs);
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    // Request NMI
    _busSockets[busSocket].irqDurationTStates = (durationTStates <= 0) ? BR_IRQ_PULSE_T_STATES : durationTStates;
    _busSockets[busSocket].irqPending = true;
}

// Bus request
void BusAccess::targetReqBus(int busSocket, BR_BUS_ACTION_REASON busMasterReason)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
    {
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetReqBus sock %d invalid count = %d", busSocket, _busSocketCount);
        return;
    }
    _busSockets[busSocket].busMasterRequest = true;
    _busSockets[busSocket].busMasterReason = busMasterReason;

    // Bus request can be handled immediately as the BUSRQ line is not part of the multiplexer (so is not affected by WAIT processing)
    busActionCheck();
    busActionHandleStart();
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "reqBus sock %d enabled %d reason %d", busSocket, _busSockets[busSocket].enabled, busMasterReason);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paging
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Paging for injection
void BusAccess::targetPageForInjection(int busSocket, bool pageOut)
{
    // Keep track of delayed paging requests
    if (pageOut)
    {
        
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetPageForInjection TRUE count %d en %s", _busSocketCount,
        //             (_busSocketCount > 0) ? (_busSockets[0].enabled ? "Y" : "N") : "X");

        // Tell all connected devices to page in/out
        for (int i = 0; i < _busSocketCount; i++)
        {
            if (_busSockets[i].enabled)
            {
                _busSockets[i].busActionCallback(_busSockets[i].pSourceObject, 
                        BR_BUS_ACTION_PAGE_OUT_FOR_INJECT, BR_BUS_ACTION_GENERAL);
            }
        }
    }
    else
    {
        // LogWrite(MODULE_PREFIX, LOG_DEBUG, "targetPageForInjection FALSE on complete count %d en %s", _busSocketCount,
        //             (_busSocketCount > 0) ? (_busSockets[0].enabled ? "Y" : "N") : "X");

        _targetPageInOnReadComplete = true;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::service()
{
#ifndef TIMER_BASED_WAIT_STATES

#ifndef SERVICE_PERFORMANCE_OPTIMIZED

    serviceWaitActivity();

#else

    if (_busServiceEnabled)
    {
        uint32_t serviceLoopStartUs = micros();
        for (int i = 0; i < 10; i++)
        {
            busCycleService();

            if ((_busActionState != BUS_ACTION_STATE_ASSERTED) && (!_waitAsserted))
                break;

            if (isTimeout(micros(), serviceLoopStartUs, BR_MAX_TIME_IN_SERVICE_LOOP_US))
                break;
        }
    }

#endif
#endif

    // Service target controller
    _targetController.service();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Actions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::busActionCheck()
{
    // Only check for new stuff if idle
    if (_busActionState != BUS_ACTION_STATE_NONE)
        return;

    // See if anything to do
    int busSocket = -1;
    for (int i = 0; i < _busSocketCount; i++)
    {
        if (_busSockets[i].enabled && (_busSockets[i].getType() != BR_BUS_ACTION_NONE))
        {
            busSocket = i;
            break;
        }
    }
    if (busSocket < 0)
        return;

    // Set this new action as in progress
    _busActionSocket = busSocket;
    _busActionType = _busSockets[busSocket].getType();
    _busActionState = BUS_ACTION_STATE_PENDING;
    _busActionInProgressStartUs = micros();
}

bool BusAccess::busActionHandleStart()
{
    // Check if a bus action has started but isn't yet asserted
    if (_busActionState != BUS_ACTION_STATE_PENDING)
        return false;

    // Initiate the action
    setSignal(_busActionType, true);

    // TODO
    // if (_busActionType == BR_BUS_ACTION_RESET)
    //     LogWrite(MODULE_PREFIX, LOG_DEBUG, "RESET start %u", micros());

    // Set start timer
    _busActionAssertedStartUs = micros();
    _busActionAssertedMaxUs = _busSockets[_busActionSocket].getAssertUs(_busActionType, clockCurFreqHz());
    _busActionState = BUS_ACTION_STATE_ASSERTED;

#ifdef DEBUG_IORQ_PROCESSING
    // TODO
    if (_busActionType == BR_BUS_ACTION_IRQ)
    {
        ISR_VALUE(ISR_ASSERT_CODE_DEBUG_D, _busActionAssertedMaxUs);
    }
    else if (_busActionType == BR_BUS_ACTION_RESET)
    {
        _statusInfo._debugIORQIntProcessed = false;
        _statusInfo._debugIORQNum = 0;
        _statusInfo._debugIORQIntNext = false;
        ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_A);
    }
#endif

    return true;
}

bool BusAccess::busAccessHandleIrqAck()
{
    // Check for IRQ action
    if (_busActionType == BR_BUS_ACTION_IRQ)
    {
        // Check for irq ack
        uint32_t ctrlBusVals = controlBusRead();

        // Check M1 and IORQ are asserted and BUSACK is not
        bool irqAck = ((ctrlBusVals & BR_CTRL_BUS_M1_MASK) && 
                (ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) &&
                ((ctrlBusVals & BR_CTRL_BUS_BUSACK_MASK) == 0));

        // A valid IRQ
        if (irqAck)
        {
            // Clear the IRQ
            setSignal(_busActionType, false);
            busActionClearFlags();

#ifdef DEBUG_IORQ_PROCESSING
            if (!_statusInfo._debugIORQIntProcessed)
            {
                _statusInfo._debugIORQIntNext = false;
                _statusInfo._debugIORQNum = 0;
                    ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_H);
                if (isTimeout(micros(), _statusInfo._debugIORQClrMicros, 1000000))
                {
                    _statusInfo._debugIORQIntProcessed = true;
                }
            }
            else if ((!_statusInfo._debugIORQIntNext) && (_statusInfo._debugIORQNum != 0))
            {
                    ISR_VALUE(ISR_ASSERT_CODE_DEBUG_I, _statusInfo._debugIORQNum+1000);
                _statusInfo._debugIORQIntNext = true; 
                _statusInfo._debugIORQMatchNum = 0;
                _statusInfo._debugIORQMatchExpected = _statusInfo._debugIORQNum;
            }
            else
            {
                if (_statusInfo._debugIORQMatchNum != _statusInfo._debugIORQMatchExpected)
                {
                    // TODO
                    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
                    // lowlev_cycleDelay(20);
                    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
                    // lowlev_cycleDelay(20);
                    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
                    // lowlev_cycleDelay(20);
                    // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
                    // ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_K);
                }
                _statusInfo._debugIORQMatchNum = 0;
            }

            // // TODO
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 1);
            // lowlev_cycleDelay(20);
            // digitalWrite(BR_DEBUG_PI_SPI0_CE0, 0);
#endif // DEBUG_IORQ_PROCESSING

        }

    }
    return false;
}

void BusAccess::busActionHandleActive()
{
    // Timeout on overall action
    if (_busActionState == BUS_ACTION_STATE_PENDING)
    {
        if (isTimeout(micros(), _busActionInProgressStartUs, MAX_WAIT_FOR_PENDING_ACTION_US))
        {
            // Cancel the request
            // LogWrite(MODULE_PREFIX, LOG_DEBUG, "Timeout on bus action %u type %d _waitAsserted %d", micros(), _busActionType, _waitAsserted);
            setSignal(_busActionType, false);
            busActionClearFlags();
        }
    }
    
    // Handle active asserts only
    if (_busActionState != BUS_ACTION_STATE_ASSERTED)
        return;

    // Handle BUSRQ separately
    if (_busActionType == BR_BUS_ACTION_BUSRQ)
    {
        // Check for BUSACK
        if (controlBusReqAcknowledged())
        {
            // Take control of bus
            controlTake();

            // Clear the action now so that any new action raised by the callback
            // such as a reset, etc can be asserted before BUSRQ is released
            busActionClearFlags();

            // Callback
            busActionCallback(BR_BUS_ACTION_BUSRQ, _busSockets[_busActionSocket].busMasterReason);

            // Release bus
            controlRelease();
        }
        else
        {
            // Check for timeout on BUSACK
            if (isTimeout(micros(), _busActionAssertedStartUs, _busActionAssertedMaxUs))
            {
                // For bus requests a timeout means failure
                busActionCallback(BR_BUS_ACTION_BUSRQ_FAIL, _busSockets[_busActionSocket].busMasterReason);
                setSignal(BR_BUS_ACTION_BUSRQ, false);
                busActionClearFlags();
            }
        }

    }
    else
    {
        // Check for IRQ acknowledged
        if (!busAccessHandleIrqAck())
        {
            // Handle ending RESET, INT and NMI bus actions
            if (isTimeout(micros(), _busActionAssertedStartUs, _busActionAssertedMaxUs))
            {

                busActionCallback(_busActionType, BR_BUS_ACTION_GENERAL);
                setSignal(_busActionType, false);
                busActionClearFlags();
                if (_busActionType == BR_BUS_ACTION_RESET)
                    busActionCallback(BR_BUS_ACTION_RESET_END, BR_BUS_ACTION_GENERAL);
            }
        }
    }
}

void BusAccess::busActionClearFlags()
{
    // Clear for all sockets
    for (int i = 0; i < _busSocketCount; i++)
        _busSockets[i].clearDown(_busActionType);
    _busActionState = BUS_ACTION_STATE_NONE;
}

void BusAccess::busActionCallback(BR_BUS_ACTION busActionType, BR_BUS_ACTION_REASON reason)
{
    for (int i = 0; i < _busSocketCount; i++)
    {
        if (!_busSockets[i].enabled)
            continue;
        // Inform all active sockets of the bus action completion
        if (_busSockets[i].busActionCallback)
            _busSockets[i].busActionCallback(_busSockets[i].pSourceObject, busActionType, reason);
    }

    // If we just programmed then call again for mirroring
    if (reason == BR_BUS_ACTION_PROGRAMMING)
    {
        for (int i = 0; i < _busSocketCount; i++)
        {
            if (!_busSockets[i].enabled)
                continue;
            if (_busSockets[i].busActionCallback)
                _busSockets[i].busActionCallback(_busSockets[i].pSourceObject, busActionType, BR_BUS_ACTION_MIRROR);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer Interrupt Service Routine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::stepTimerISR( void* pParam)
{
#ifdef ISR_TEST
    // write32(ARM_GPIO_GPSET0, 1 << BR_DEBUG_PI_SPI0_CE0);  
    // lowlev_cycleDelay(20);
    // write32(ARM_GPIO_GPCLR0, 1 << BR_DEBUG_PI_SPI0_CE0);  
    // write32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);
    lowlev_disable_fiq();
#else
#ifdef TIMER_BASED_WAIT_STATES
    if (_busServiceEnabled)
        serviceWaitActivity();
#endif
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sevice bus activity
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::serviceWaitActivity()
{
    // Check for new bus actions
    busActionCheck();

    // Handle completion of any existing bus actions
    busActionHandleActive();

    // Check if we are already in a wait
    if (!_waitAsserted)
    {
        // We're not currently in wait so read the control lines to check for a new one
        uint32_t busVals = read32(ARM_GPIO_GPLEV0);

        // Check if we have a new wait (and we're not in BUSACK)
        if (((busVals & BR_WAIT_BAR_MASK) == 0) && ((busVals & BR_BUSACK_BAR_MASK) != 0))
        {
            // Record the time of the wait start
            _waitAssertedStartUs = micros();
            _waitAsserted = true;

            // Handle the wait
            waitHandleNew();
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
    if (_waitAsserted)
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
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read release
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitHandleReadRelease()
{
    if (!_targetReadInProgress)
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
            // Set data bus direction in
            pibSetIn();
            write32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);
            // Check if paging in/out is required
            if (_targetPageInOnReadComplete)
            {
                busAccessCallbackPageIn();
                _targetPageInOnReadComplete = false;
            }

            // Done now
            return;
        }
    }
    _targetReadInProgress = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// New Wait
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitHandleNew()
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

    // Elapsed and count
    uint32_t isrElapsedUs = micros() - isrStartUs;
    _statusInfo.isrCount++;

    // Stats
    if (ctrlBusVals & BR_CTRL_BUS_MREQ_MASK)
    {
        if (ctrlBusVals & BR_CTRL_BUS_RD_MASK)
            _statusInfo.isrMREQRD++;
        else if (ctrlBusVals & BR_CTRL_BUS_WR_MASK)
            _statusInfo.isrMREQWR++;
    }
    else if (ctrlBusVals & BR_CTRL_BUS_IORQ_MASK)
    {
        if (ctrlBusVals & BR_CTRL_BUS_RD_MASK)
            _statusInfo.isrIORQRD++;
        else if (ctrlBusVals & BR_CTRL_BUS_WR_MASK)
            _statusInfo.isrIORQWR++;
        else if (ctrlBusVals & BR_CTRL_BUS_M1_MASK)
            _statusInfo.isrIRQACK++;
    }

    // Overflows
    if (_statusInfo.isrAccumUs > 1000000000)
    {
        _statusInfo.isrAccumUs = 0;
        _statusInfo.isrAvgingCount = 0;
    }

    // Averages
    if (isrElapsedUs < 1000000)
    {
        _statusInfo.isrAccumUs += isrElapsedUs;
        _statusInfo.isrAvgingCount++;
        _statusInfo.isrAvgNs = _statusInfo.isrAccumUs * 1000 / _statusInfo.isrAvgingCount;
    }

    // Max
    if (_statusInfo.isrMaxUs < isrElapsedUs)
        _statusInfo.isrMaxUs = isrElapsedUs;

}

char BusAccessStatusInfo::_jsonBuf[MAX_JSON_LEN];
const char* BusAccessStatusInfo::getJson()
{
    char tmpResp[MAX_JSON_LEN];
    snprintf(_jsonBuf, sizeof(tmpResp), "\"err\":\"ok\",\"c\":%u,\"avgNs\":%u,\"maxUs\":%u,\"clrAvgNs\":%u,\"clrMaxUs\":%u,\"busRqFail\":%u,\"busActFailDueWait\":%u",
                isrCount, isrAvgNs, isrMaxUs, clrAvgNs, clrMaxUs, busrqFailCount, busActionFailedDueToWait);
    snprintf(tmpResp, sizeof(tmpResp), ",\"mreqRd\":%u,\"mreqWr\":%u,\"iorqRd\":%u,\"iorqWr\":%u,\"irqAck\":%u,\"isrBadBusrq\":%u,\"irqDuringBusAck\":%u,\"irqNoWait\":%u",
                isrMREQRD, isrMREQWR, isrIORQRD, isrIORQWR, isrIRQACK, isrSpuriousBUSRQ, isrDuringBUSACK, isrWithoutWAIT);
    strlcat(_jsonBuf, tmpResp, MAX_JSON_LEN);

#ifdef DEBUG_IORQ_PROCESSING
    snprintf(_jsonBuf, sizeof(tmpResp), "");
    snprintf(tmpResp, sizeof(tmpResp), ",\"debugIORQCount\":%u,\"debugIORQList\":[", _debugIORQNum);
    strlcat(_jsonBuf, tmpResp, MAX_JSON_LEN);
    for (int i = 0; i < _debugIORQNum; i++)
    {
        if (i != 0)
            strlcat(_jsonBuf, ",", MAX_JSON_LEN);
        snprintf(tmpResp, sizeof(tmpResp), "{\"u\":%u,\"a\":%02x,\"d\":%02x,\"f\":%04x,\"c\":%u}", 
                    _debugIORQEvs[i]._micros,
                    _debugIORQEvs[i]._addr,
                    _debugIORQEvs[i]._data,
                    _debugIORQEvs[i]._flags,
                    _debugIORQEvs[i]._count
                    );
        strlcat(_jsonBuf, tmpResp, MAX_JSON_LEN);
    }
    strlcat(_jsonBuf, "]", MAX_JSON_LEN);

    _debugIORQIntProcessed = false;
    _debugIORQIntNext = false;
    _debugIORQNum = 0;
    _debugIORQClrMicros = micros();
#endif

    return _jsonBuf;
}


