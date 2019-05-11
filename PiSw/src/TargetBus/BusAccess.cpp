// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "../System/piwiring.h"
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

// Bus sockets
BusSocketInfo BusAccess::_busSockets[MAX_BUS_SOCKETS];
int BusAccess::_busSocketCount = 0;

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
volatile bool BusAccess::_busActionInProgress = false;
volatile bool BusAccess::_busActionAsserted = false;
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
    
    // Bus Request
    setPinOut(BR_BUSRQ_BAR, 1);
    _busIsUnderControl = false;
        
    // Address push
    setPinOut(BR_PUSH_ADDR_BAR, 1);
    
    // High address clock
    setPinOut(BR_HADDR_CK, 0);
    
    // Low address clock
    setPinOut(BR_LADDR_CK, 0);
    
    // Data bus direction
    setPinOut(BR_DATA_DIR_IN, 1);

    // Paging initially inactive
    setPinOut(BR_PAGING_RAM_PIN, 0);

    // Setup MREQ and IORQ enables
    waitSetupMREQAndIORQEnables();
    _waitAsserted = false;

    // TODO maybe no longer needed ??? service or timer used instead of edge detection??
    // Remove edge detection
    // WR32(ARM_GPIO_GPREN0, 0);
    // WR32(ARM_GPIO_GPAREN0, 0);
    // WR32(ARM_GPIO_GPFEN0, 0);
    // WR32(ARM_GPIO_GPAFEN0, 0); //RD32(ARM_GPIO_GPFEN0) & (~(1 << BR_WAIT_BAR)));

    // // Clear any currently detected edge
    // WR32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);

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
    for (int i = 0; i < BusSocketInfo::MAX_WAIT_FOR_BUSACK_US; i++)
    {
        if (!controlBusAcknowledged())
            break;
        microsDelay(1);
    }

    // Clear any wait condition if necessary
    uint32_t busVals = RD32(ARM_GPIO_GPLEV0);
    if ((busVals & BR_WAIT_BAR_MASK) == 0)
        waitResetFlipFlops();

    // Update wait state generation
    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "busAccessReset");
    waitEnablementUpdate();
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
void BusAccess::targetReqReset(int busSocket, int durationUs)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].busActionDurationUs = (durationUs <= 0) ? BR_RESET_PULSE_US : durationUs;
    _busSockets[busSocket].busActionRequested = BR_BUS_ACTION_RESET;
}

// Non-maskable interrupt the host
void BusAccess::targetReqNMI(int busSocket, int durationUs)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].busActionDurationUs = (durationUs <= 0) ? BR_NMI_PULSE_US : durationUs;
    _busSockets[busSocket].busActionRequested = BR_BUS_ACTION_NMI;
}

// Maskable interrupt the host
void BusAccess::targetReqIRQ(int busSocket, int durationUs)
{
    // Check validity
    // LogWrite("BA", LOG_DEBUG, "ReqIRQ sock %d us %d", busSocket, _busSockets[busSocket].busActionDurationUs);
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].busActionDurationUs = (durationUs <= 0) ? BR_IRQ_PULSE_US : durationUs;
    _busSockets[busSocket].busActionRequested = BR_BUS_ACTION_IRQ;
}

// Bus request
void BusAccess::targetReqBus(int busSocket, BR_BUS_ACTION_REASON busMasterReason)
{
    // Check validity
    if ((busSocket < 0) || (busSocket >= _busSocketCount))
        return;
    _busSockets[busSocket].busMasterRequest = true;
    _busSockets[busSocket].busMasterReason = busMasterReason;

    // Bus request can be handled immediately
    busActionCheck();
    busActionAssertStart();
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
    serviceWaitActivity();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Actions
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::busActionCheck()
{
    // Should be idle
    if (_busActionInProgress)
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
    _busActionInProgress = true;
    _busActionInProgressStartUs = micros();
    _busActionAsserted = false;
}

bool BusAccess::busActionAssertStart()
{
    // Check if a bus action has started but isn't yet asserted
    if (!_busActionInProgress || _busActionAsserted)
        return false;

    // Initiate the action
    setSignal(_busActionType, true);

    // Set start timer
    _busActionAssertedStartUs = micros();
    _busActionAssertedMaxUs = _busSockets[_busActionSocket].getAssertUs();
    _busActionAsserted = true;
    return true;
}

void BusAccess::busActionAssertActive()
{
    // Handle active asserts
    if (!_busActionInProgress || !_busActionAsserted)
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
        if (isTimeout(micros(), _busActionAssertedStartUs, _busActionAssertedMaxUs))
        {
            busActionCallback(_busActionType, BR_BUS_ACTION_GENERAL);
            
            setSignal(_busActionType, false);
            busActionClearFlags();
        }
    }
}

void BusAccess::busActionClearFlags()
{
    // Clear for all sockets
    for (int i = 0; i < _busSocketCount; i++)
        _busSockets[i].clear(_busActionType);
    _busActionInProgress = false;
}

void BusAccess::busActionCallback(BR_BUS_ACTION busActionType, BR_BUS_ACTION_REASON reason)
{
    for (int i = 0; i < _busSocketCount; i++)
    {
        if (!_busSockets[i].enabled)
            continue;
        // Inform all active sockets of the bus action completion
        _busSockets[i].busActionCallback(busActionType, reason);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer Interrupt Service Routine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::stepTimerISR([[maybe_unused]] void* pParam)
{
#ifdef TIMER_BASED_WAIT_STATES
    serviceWaitActivity();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer Interrupt Service Routine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::serviceWaitActivity()
{
    // Read the control lines
    uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

    // Check for timeouts on bus actions
    if (_busActionInProgress)
    {
        // Handle signals that have already started
        busActionAssertActive();

        // Timeout on overall action
        if (isTimeout(micros(), _busActionInProgressStartUs, MAX_WAIT_FOR_PENDING_ACTION_US))
        {
            // Cancel the request
            busActionClearFlags();
        }
    }

    // See if a new bus action is requested
    busActionCheck();

    // Handle wait
    if (!_waitAsserted)
    {
        // Start any bus actions here
        busActionAssertStart();

        // Check if we have a new wait (and we're not in BUSACK)
        if (((busVals & BR_WAIT_BAR_MASK) == 0) && ((busVals & BR_BUSACK_BAR_MASK) != 0))
        {
            // Check if a bus action (other than BUSRQ) is happening which would 
            // be cleared too soon by the wait handler as MUX hardware is shared
            if (_busActionInProgress && (_busActionType != BR_BUS_ACTION_BUSRQ))
            {
                // Don't handle the wait until the bus action has completed
            }
            else
            {
                // In case we are held in the wait
                _waitAssertedStartUs = micros();
                _waitAsserted = true;

                // Handle the wait
                waitHandleNew();
            }
        }
    }

    // Check if wait is asserted
    if (_waitAsserted)
    {
        // Read the control lines
        busVals = RD32(ARM_GPIO_GPLEV0);

        // IORQ waits can be released as we don't hold at an IORQ
        if ((busVals & BR_IORQ_BAR_MASK) == 0)
        {
            // Release the wait - also clears _waitAsserted flag
            waitResetFlipFlops();
        }
        else if (!_waitHold)
        {
            // If we are not held then check for timeouts
            if (isTimeout(micros(), _waitAssertedStartUs, _waitCycleLengthUs))
            {
                // Check if we need to assert any new bus requests
                busActionAssertStart();

                // Release the wait - also clears _waitAsserted flag
                waitResetFlipFlops();
            }
        }
    }

    // Handle the end of a read cycle
    if (!_waitAsserted && _targetReadInProgress)
    {
        // Stay here until read cycle is complete
        uint32_t waitForReadCompleteStartUs = micros();
        while(!isTimeout(micros(), waitForReadCompleteStartUs, MAX_WAIT_FOR_END_OF_READ_US))
        {
            // Read the control lines
            busVals = RD32(ARM_GPIO_GPLEV0);

            // Check if a neither IORQ or MREQ asserted
            if (((busVals & BR_MREQ_BAR_MASK) != 0) && ((busVals & BR_IORQ_BAR_MASK) != 0))
            {
                // Set data bus direction in
                pibSetIn();
                WR32(ARM_GPIO_GPSET0, 1 << BR_DATA_DIR_IN);
                _targetReadInProgress = false;
                // Check if paging in/out is required
                if (_targetPageInOnReadComplete)
                {
                    pagingPageIn();
                    _targetPageInOnReadComplete = false;
                }
                // Done now
                break;
            }
        }
    }
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
        // Read the control lines
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);

        // Get the appropriate bits for up-line communication
        ctrlBusVals = 
                (((busVals & BR_RD_BAR_MASK) == 0) ? BR_CTRL_BUS_RD_MASK : 0) |
                (((busVals & BR_WR_BAR_MASK) == 0) ? BR_CTRL_BUS_WR_MASK : 0) |
                (((busVals & BR_MREQ_BAR_MASK) == 0) ? BR_CTRL_BUS_MREQ_MASK : 0) |
                (((busVals & BR_IORQ_BAR_MASK) == 0) ? BR_CTRL_BUS_IORQ_MASK : 0) |
                (((busVals & BR_WAIT_BAR_MASK) == 0) ? BR_CTRL_BUS_WAIT_MASK : 0);

        // Check for (MREQ || IORQ) && (RD || WR)
        bool ctrlValid = ((ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) || (ctrlBusVals & BR_CTRL_BUS_MREQ_MASK)) && ((ctrlBusVals & BR_CTRL_BUS_RD_MASK) || (ctrlBusVals & BR_CTRL_BUS_WR_MASK));
        
        // Also valid if IORQ && M1 as this is used for interrupt ack
        // TODO - this isn't valid as M1 can't be relied upon
        // ctrlValid = ctrlValid || (ctrlBusVals & BR_CTRL_BUS_IORQ_MASK) & (ctrlBusVals & BR_CTRL_BUS_M1_MASK);

        // If ctrl is already valid then continue
        if (ctrlValid)
            break;

        // Delay
        microsDelay(1);

        // Ensure we don't lock up on weird bus activity
        avoidLockupCtr++;
    }
    
    // Check if bus detail is suspended for one cycle
    if (_waitSuspendBusDetailOneCycle)
    {
        _waitSuspendBusDetailOneCycle = false;
    }
    else
    {

    // TODO DEBUG
            // pinMode(BR_WR_BAR, INPUT);
            // pinMode(BR_RD_BAR, INPUT);
            // pinMode(BR_MREQ_BAR, INPUT);
            // pinMode(BR_IORQ_BAR, INPUT);

        // Clear the mux to deactivate output enables
        muxClear();

        // Set PIB to input
        pibSetIn();

        // Set data bus direction out so we can read the M1 value
        WR32(ARM_GPIO_GPCLR0, 1 << BR_DATA_DIR_IN);

        // Read M1
        microsDelay(1);

        // Read the control lines and set M1
        uint32_t busVals = RD32(ARM_GPIO_GPLEV0);
        ctrlBusVals |= (((busVals & BR_M1_PIB_BAR_MASK) == 0) ? BR_CTRL_BUS_M1_MASK : 0);

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

        // Enable data bus onto PIB
        digitalWrite(BR_DATA_DIR_IN, 1);
        muxSet(BR_MUX_DATA_OE_BAR_LOW);

        // Delay to allow data to settle
        lowlev_cycleDelay(CYCLES_DELAY_FOR_READ_FROM_PIB);

        // Read the data bus values
        dataBusVals = pibGetValue() & 0xff;

        // Clear Mux
        muxClear();
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

    // If Z80 is reading from the data bus (inc reading an ISR vector)
    // and result is valid then put the returned data onto the bus
    bool isWriting = (ctrlBusVals & BR_CTRL_BUS_WR_MASK);
    if (!isWriting && ((retVal & BR_MEM_ACCESS_RSLT_NOT_DECODED) == 0))
    {
        // Debug
        // ISR_ASSERT(ISR_ASSERT_CODE_DEBUG_I);
        // Now driving data onto the target data bus
        digitalWrite(BR_DATA_DIR_IN, 0);
        // A flip-flop handles data OE during the IORQ/MREQ cycle and 
        // deactivates at the end of that cycle - so prime this flip-flop here
        muxSet(BR_MUX_DATA_OE_BAR_LOW);
        pibSetOut();
        pibSetValue(retVal & 0xff);
        // lowlev_cycleDelay(CYCLES_DELAY_FOR_WAIT_CLEAR);
        muxClear();
        lowlev_cycleDelay(CYCLES_DELAY_FOR_TARGET_READ);
        _targetReadInProgress = true;
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
    return _jsonBuf;
}
