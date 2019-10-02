// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "../System/PiWiring.h"
#include "../System/CInterrupts.h"
#include "../System/lowlev.h"
#include "../System/lowlib.h"
#include "../System/BCM2835.h"
#include "../System/logging.h"
#include "../System/Timers.h"
#include "../System/ee_sprintf.h"

// Uncomment to drive wait states through timer - alternatively use service()
// #define TIMER_BASED_WAIT_STATES 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Module name
static const char FromBusAccess[] = "BusAccess";

// Hardware version
int BusAccess::_hwVersionNumber = HW_VERSION_DEFAULT;

// Bus sockets
BusSocketInfo BusAccess::_busSockets[MAX_BUS_SOCKETS];
int BusAccess::_busSocketCount = 0;

// Bus service enabled - can be disabled to allow external API to completely control bus
bool BusAccess::_busServiceEnabled = true;

// Wait state enables
bool BusAccess::_waitOnMemory = false;
bool BusAccess::_waitOnIO = false;

// Wait is asserted (processor held)
bool volatile BusAccess::_waitAsserted = false;

// Rate at which wait is released when free-cycling
volatile uint32_t BusAccess::_waitCycleLengthUs = 1;
volatile uint32_t BusAccess::_waitAssertedStartUs = 0;

// Held in wait state
volatile bool BusAccess::_waitHold = false;

// Wait suspend bus detail for one cycle
volatile bool BusAccess::_waitSuspendBusDetailOneCycle = false;

// Bus action handling
volatile int BusAccess::_busActionSocket = 0;
volatile BR_BUS_ACTION BusAccess::_busActionType = BR_BUS_ACTION_NONE;
volatile uint32_t BusAccess::_busActionInProgressStartUs = 0;
volatile uint32_t BusAccess::_busActionAssertedStartUs = 0;
volatile uint32_t BusAccess::_busActionAssertedMaxUs = 0;
volatile BusAccess::BUS_ACTION_STATE BusAccess::_busActionState = BUS_ACTION_STATE_NONE;
volatile bool BusAccess::_busActionSyncWithWait = false;

// Status
BusAccessStatusInfo BusAccess::_statusInfo;

// Paging
bool volatile BusAccess::_targetPageInOnReadComplete = false;

// Debug
int volatile BusAccess::_isrAssertCounts[ISR_ASSERT_NUM_CODES];

// Target read in progress
bool volatile BusAccess::_targetReadInProgress = false;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Initialise the bus raider
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
    _busIsUnderControl = false;
        
    // High address clock
    setPinOut(BR_HADDR_CK, 0);
    
    // Data bus driver direction inward
    setPinOut(BR_DATA_DIR_IN, 1);

    // Paging initially inactive
    setPinOut(BR_PAGING_RAM_PIN, 0);

    // Setup MREQ and IORQ enables
    waitSetupMREQAndIORQEnables();
    _waitAsserted = false;

    // Remove edge detection
#ifdef ISR_TEST
    WR32(ARM_GPIO_GPREN0, 0);
    WR32(ARM_GPIO_GPAREN0, 0);
    WR32(ARM_GPIO_GPFEN0, 0);
    WR32(ARM_GPIO_GPAFEN0, 0); //RD32(ARM_GPIO_GPFEN0) & (~(1 << BR_WAIT_BAR)));

    // // Clear any currently detected edge
    WR32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);

    CInterrupts::connectFIQ(52, stepTimerISR, 0);
#endif

    // Heartbeat timer
#ifdef TIMER_BASED_WAIT_STATES
    Timers::set(TIMER_ISR_PERIOD_US, BusAccess::stepTimerISR, NULL);
    Timers::start();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Reset
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reset the bus raider bus
void BusAccess::busAccessReset()
{
    // Instruct hardware to page in
    pagingPageIn();
    
    // Clear mux
    muxClear();

    // Wait cycle length
    _waitCycleLengthUs = 0;

    // Check for bus ack released
    waitForBusAck(false);

    // Clear any wait condition if necessary
    uint32_t busVals = RD32(ARM_GPIO_GPLEV0);
    if ((busVals & BR_WAIT_BAR_MASK) == 0)
        waitResetFlipFlops();

    // Update wait state generation
    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "busAccessReset");
    waitEnablementUpdate();

    // Reactivate bus service
    _busServiceEnabled = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait System
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitOnMemory(int busSocket, bool isOn)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].waitOnMemory = isOn;

    // Update wait handling
    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "waitonMem");
    waitEnablementUpdate();
}

void BusAccess::waitOnIO(int busSocket, bool isOn)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].waitOnIO = isOn;

    // Update wait handling
    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "waitOnIO");
    waitEnablementUpdate();
}

bool BusAccess::waitIsOnMemory()
{
    return _waitOnMemory;
}

// Min cycle Us when in waitOnMemory mode
void BusAccess::waitSetCycleUs(uint32_t cycleUs)
{
    _waitCycleLengthUs = cycleUs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pause & Single Step Handling
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitRelease()
{
    waitResetFlipFlops();
}

bool BusAccess::waitIsHeld()
{
    return _waitHold;
}

void BusAccess::waitHold(int busSocket, bool hold)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].holdInWaitReq = hold;
    // Update flag
    for (int i = 0; i < _busSocketCount; i++)
    {
        if (!_busSockets[i].enabled)
            continue;
        if (_busSockets[i].holdInWaitReq)
        {
            _waitHold = true;
            return;
        }
    }
    _waitHold = false;
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
    LogWrite("BusAccess", LOG_DEBUG, "targetReqReset");
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
    // LogWrite("BA", LOG_DEBUG, "ReqIRQ sock %d us %d", busSocket, _busSockets[busSocket].busActionDurationUs);
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
        return;
    _busSockets[busSocket].busMasterRequest = true;
    _busSockets[busSocket].busMasterReason = busMasterReason;

    // Bus request can be handled immediately as the BUSRQ line is not part of the multiplexer (so is not affected by WAIT processing)
    busActionCheck();
    busActionHandleStart();
    // LogWrite("BusAccess", LOG_DEBUG, "reqBus sock %d enabled %d reason %d", busSocket, _busSockets[busSocket].enabled, busMasterReason);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Paging
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Paging for injection
void BusAccess::targetPageForInjection([[maybe_unused]]int busSocket, bool pageOut)
{
    // Keep track of delayed paging requests
    if (pageOut)
    {
        
        // LogWrite("BA", LOG_DEBUG, "targetPageForInjection TRUE count %d en %s", _busSocketCount,
        //             (_busSocketCount > 0) ? (_busSockets[0].enabled ? "Y" : "N") : "X");

        // Tell all connected devices to page in/out
        for (int i = 0; i < _busSocketCount; i++)
        {
            if (_busSockets[i].enabled)
            {
                _busSockets[i].busActionCallback(BR_BUS_ACTION_PAGE_OUT_FOR_INJECT, BR_BUS_ACTION_GENERAL);
            }
        }
    }
    else
    {
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
        for (int i = 0; i < 100; i++)
        {
            serviceWaitActivity();

            if ((_busActionState != BUS_ACTION_STATE_ASSERTED) && (!_waitAsserted))
                break;

            if (isTimeout(micros(), serviceLoopStartUs, BR_MAX_TIME_IN_SERVICE_LOOP_US))
                break;
        }
    }

#endif
#endif
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
    //     LogWrite(FromBusAccess, LOG_DEBUG, "RESET start %u", micros());

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
        // Read the control lines
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

        // Check M1 and IORQ are low and BUSACK is not
        bool irqAck = (((busVals & BR_V20_M1_BAR_MASK) == 0) && 
                ((busVals & BR_IORQ_BAR_MASK) == 0) &&
                ((busVals & BR_BUSACK_BAR_MASK) != 0));
        if (_hwVersionNumber == 17)
        {
            // TODO check if we can do better than this
            irqAck = false;
        }

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
            // LogWrite(FromBusAccess, LOG_DEBUG, "Timeout on bus action %u type %d _waitAsserted %d", micros(), _busActionType, _waitAsserted);
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
        if (controlBusAcknowledged())
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
            _busSockets[i].busActionCallback(busActionType, reason);
    }

    // If we just programmed then call again for mirroring
    if (reason == BR_BUS_ACTION_PROGRAMMING)
    {
        for (int i = 0; i < _busSocketCount; i++)
        {
            if (!_busSockets[i].enabled)
                continue;
            if (_busSockets[i].busActionCallback)
                _busSockets[i].busActionCallback(busActionType, BR_BUS_ACTION_MIRROR);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer Interrupt Service Routine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::stepTimerISR([[maybe_unused]] void* pParam)
{
#ifdef ISR_TEST
    // WR32(ARM_GPIO_GPSET0, 1 << BR_DEBUG_PI_SPI0_CE0);  
    // lowlev_cycleDelay(20);
    // WR32(ARM_GPIO_GPCLR0, 1 << BR_DEBUG_PI_SPI0_CE0);  
    // WR32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);
    lowlev_disable_fiq();
#else
#ifdef TIMER_BASED_WAIT_STATES
    if (_busServiceEnabled)
        serviceWaitActivity();
#endif
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer Interrupt Service Routine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::serviceWaitActivity()
{
    // Check for new bus actions
    busActionCheck();

    // Handle completion of any existing bus actions
    busActionHandleActive();
    if (_busActionState == BUS_ACTION_STATE_ASSERTED)
        return;

    // Check for no existing wait
    if (!_waitAsserted)
    {
        // Read the control lines
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

        // Check if we have a new wait (and we're not in BUSACK)
        if (((busVals & BR_WAIT_BAR_MASK) == 0) && ((busVals & BR_BUSACK_BAR_MASK) != 0))
        {
            // In case we are held in the wait
            _waitAssertedStartUs = micros();
            _waitAsserted = true;

            // Handle the wait
            waitHandleNew();
        }
        else
        {
            if (!waitIsOnMemory())
            {
                // Check if we need to assert any new bus requests
                busActionHandleStart();
            }
        }
    }

    // Handle existing waits (including ones just started)
    if (_waitAsserted)
    {
        // Read the control lines
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

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

            // Release the wait - also clears _waitAsserted flag
            waitResetFlipFlops();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Read release
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitHandleReadRelease()
{
    // Stay here until read cycle is complete
    uint32_t waitForReadCompleteStartUs = micros();
    while(!isTimeout(micros(), waitForReadCompleteStartUs, MAX_WAIT_FOR_END_OF_READ_US))
    {
        // Read the control lines
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

        // Check if a neither IORQ or MREQ asserted
        if (((busVals & BR_MREQ_BAR_MASK) != 0) && ((busVals & BR_IORQ_BAR_MASK) != 0))
        {
            // Set data bus direction in
            pibSetIn();
            WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);
            // Check if paging in/out is required
            if (_targetPageInOnReadComplete)
            {
                pagingPageIn();
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
    uint32_t ctrlBusVals = 0;

    // Check if paging in/out is required
    if (_targetPageInOnReadComplete)
    {
        pagingPageIn();
        _targetPageInOnReadComplete = false;
    }

    // Loop until control lines are valid
    // In a write cycle WR is asserted after MREQ so we need to wait until WR changes before
    // we can determine what kind of operation is in progress 
    int avoidLockupCtr = 0;
    ctrlBusVals = 0;
    while(avoidLockupCtr < MAX_WAIT_FOR_CTRL_LINES_COUNT)
    {
        // Handle M1 for V1.7 hardware
        if (_hwVersionNumber == 17)
        {
            // Clear the mux to deactivate output enables
            muxClear();

            // Delay for settling of M1
            lowlev_cycleDelay(CYCLES_DELAY_FOR_M1_SETTLING);
        }

        // Read the control lines
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

        // Get the appropriate bits for up-line communication
        ctrlBusVals = 
                (((busVals & BR_RD_BAR_MASK) == 0) ? BR_CTRL_BUS_RD_MASK : 0) |
                (((busVals & BR_WR_BAR_MASK) == 0) ? BR_CTRL_BUS_WR_MASK : 0) |
                (((busVals & BR_MREQ_BAR_MASK) == 0) ? BR_CTRL_BUS_MREQ_MASK : 0) |
                (((busVals & BR_IORQ_BAR_MASK) == 0) ? BR_CTRL_BUS_IORQ_MASK : 0) |
                (((busVals & BR_WAIT_BAR_MASK) == 0) ? BR_CTRL_BUS_WAIT_MASK : 0) |
                (((busVals & BR_V20_M1_BAR_MASK) == 0) ? BR_CTRL_BUS_M1_MASK : 0);

        // Also valid if IORQ && M1 as this is used for interrupt ack
        if (_hwVersionNumber == 17)
        {
            // Clear M1 in case set above
            ctrlBusVals = ctrlBusVals & (~BR_CTRL_BUS_M1_MASK);

            // Set M1 from PIB reading
            ctrlBusVals |= (((busVals & BR_V17_M1_PIB_BAR_MASK) == 0) ? BR_CTRL_BUS_M1_MASK : 0);
        }

        // Check for (MREQ || IORQ) && (RD || WR)
        bool ctrlValid = ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) || (ctrlBusVals & BR_CTRL_BUS_MREQ_MASK)) && ((ctrlBusVals & BR_CTRL_BUS_RD_MASK) || (ctrlBusVals & BR_CTRL_BUS_WR_MASK));
        // Also valid if IORQ && M1 as this is used for interrupt ack
        ctrlValid = ctrlValid || ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) && (ctrlBusVals & BR_CTRL_BUS_M1_MASK));
        
        // If ctrl is already valid then continue
        if (ctrlValid)
            break;

        // Delay
        lowlev_cycleDelay(10);

        // Ensure we don't lock up on weird bus activity
        avoidLockupCtr++;
    }
    
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

        // Set PIB to input
        pibSetIn();

        // Clear the mux to deactivate output enables
        muxClear();
        
        // Enable the high address onto the PIB
        muxSet(BR_MUX_HADDR_OE_BAR);

        // Delay to allow data to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_HIGH_ADDR_READ);

        // Or in the high address
        addr = (pibGetValue() & 0xff) << 8;

        // Enable the low address onto the PIB
        muxSet(BR_MUX_LADDR_OE_BAR);

        // Delay to allow data to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

        // Get low address value
        addr |= pibGetValue() & 0xff;

        // Clear the mux to deactivate output enables
        muxClear();

        // Delay to allow data to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

        // Read the data bus
        // If the target machine is writing then this will be the data it wants to write
        // If reading then the memory/IO system may have placed its data onto the data bus 

        // Due to hardware limitations reading from the data bus MUST be done last as the
        // output of the data bus is latched onto the PIB (or out onto the data bus in the case
        // where the ISR provides data for the processor to read)

        // Set data bus driver direction inward - onto PIB
        WR32(ARM_GPIO_GPSET0, BR_DATA_DIR_IN_MASK);

        // Set output enable on data bus driver by resetting flip-flop
        // Note that the outputs of the data bus buffer are enabled from this point until
        // a rising edge of IORQ or MREQ
        // This can cause a bus conflict if BR_DATA_DIR_IN is set low before this happens
        muxDataBusOutputEnable();

        // Delay to allow data to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

        // Read the data bus values
        dataBusVals = pibGetValue() & 0xff;

    }

    // Send this to all bus sockets
    uint32_t retVal = BR_MEM_ACCESS_RSLT_NOT_DECODED;
    for (int sockIdx = 0; sockIdx < _busSocketCount; sockIdx++)
    {
        if (_busSockets[sockIdx].enabled && _busSockets[sockIdx].busAccessCallback)
        {
            _busSockets[sockIdx].busAccessCallback(addr, dataBusVals, ctrlBusVals, retVal);
            // TODO
            // if (ctrlBusVals & BR_CTRL_BUS_IORQ_MASK)
            //     LogWrite("BA", LOG_DEBUG, "%d IORQ %s from %04x %02x", sockIdx,
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
        WR32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);
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

        //     uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

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
    ee_sprintf(_jsonBuf, "\"err\":\"ok\",\"c\":%u,\"avgNs\":%u,\"maxUs\":%u,\"clrAvgNs\":%u,\"clrMaxUs\":%u,\"busRqFail\":%u,\"busActFailDueWait\":%u",
                isrCount, isrAvgNs, isrMaxUs, clrAvgNs, clrMaxUs, busrqFailCount, busActionFailedDueToWait);
    ee_sprintf(tmpResp, ",\"mreqRd\":%u,\"mreqWr\":%u,\"iorqRd\":%u,\"iorqWr\":%u,\"irqAck\":%u,\"isrBadBusrq\":%u,\"irqDuringBusAck\":%u,\"irqNoWait\":%u",
                isrMREQRD, isrMREQWR, isrIORQRD, isrIORQWR, isrIRQACK, isrSpuriousBUSRQ, isrDuringBUSACK, isrWithoutWAIT);
    strlcat(_jsonBuf, tmpResp, MAX_JSON_LEN);

#ifdef DEBUG_IORQ_PROCESSING
    ee_sprintf(_jsonBuf, "");
    ee_sprintf(tmpResp, ",\"debugIORQCount\":%u,\"debugIORQList\":[", _debugIORQNum);
    strlcat(_jsonBuf, tmpResp, MAX_JSON_LEN);
    for (int i = 0; i < _debugIORQNum; i++)
    {
        if (i != 0)
            strlcat(_jsonBuf, ",", MAX_JSON_LEN);
        ee_sprintf(tmpResp, "{\"u\":%u,\"a\":%02x,\"d\":%02x,\"f\":%04x,\"c\":%u}", 
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
