// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetCPU.h"
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include "lowlib.h"
#include "lowlev.h"
#include "BusSocketInfo.h"
#include "BusAccessDefs.h"
#include "TargetClockGenerator.h"

// #define ISR_TEST 1

#define V2_PROTO_USING_MUX_EN 1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status Info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusAccessStatusInfo
{
public:
    BusAccessStatusInfo()
    {
        clear();
    }
    
    void clear()
    {
        isrCount = 0;
        isrAccumUs = 0;
        isrAvgingCount = 0;
        isrAvgNs = 0;
        isrMaxUs = 0;
        isrSpuriousBUSRQ = 0;
        isrDuringBUSACK = 0;
        isrWithoutWAIT = 0;
        clrAccumUs = 0;
        clrAvgingCount = 0;
        clrAvgNs = 0;
        clrMaxUs = 0;
        busrqFailCount = 0;
        busActionFailedDueToWait = 0;
        isrMREQRD = 0;
        isrMREQWR = 0;
        isrIORQRD = 0;
        isrIORQWR = 0;
        isrIRQACK = 0;
#ifdef DEBUG_IORQ_PROCESSING
        _debugIORQNum = 0;
        _debugIORQClrMicros = 0;
        _debugIORQIntProcessed = false;
        _debugIORQIntNext = false;
        _debugIORQMatchNum = 0;
        _debugIORQMatchExpected = 0;
#endif
    }

    static const int MAX_JSON_LEN = 30 * 20;
    static char _jsonBuf[MAX_JSON_LEN];
    const char* getJson();

    // Overall ISR
    uint32_t isrCount;

    // ISR elapsed
    uint32_t isrAccumUs;
    int isrAvgingCount;
    int isrAvgNs;
    uint32_t isrMaxUs;

    // Counts
    uint32_t isrSpuriousBUSRQ;
    uint32_t isrDuringBUSACK;
    uint32_t isrWithoutWAIT;
    uint32_t isrMREQRD;
    uint32_t isrMREQWR;
    uint32_t isrIORQRD;
    uint32_t isrIORQWR;
    uint32_t isrIRQACK;

    // Clear pulse edge width
    uint32_t clrAccumUs;
    int clrAvgingCount;
    int clrAvgNs;
    uint32_t clrMaxUs;

    // BUSRQ
    uint32_t busrqFailCount;

    // Bus actions
    uint32_t busActionFailedDueToWait;

#ifdef DEBUG_IORQ_PROCESSING
    class DebugIORQ
    {
    public:
        uint32_t _micros;
        uint8_t _addr;
        uint8_t _data;
        uint16_t _flags;
        uint32_t _count;

        DebugIORQ() 
        {
            _micros = 0;
            _addr = 0;
            _data = 0;
            _flags = 0;
            _count = 0;
        }
    };

    static const int MAX_DEBUG_IORQ_EVS = 20;
    DebugIORQ _debugIORQEvs[MAX_DEBUG_IORQ_EVS];
    int _debugIORQNum;
    uint32_t _debugIORQClrMicros;
    bool _debugIORQIntProcessed;
    bool _debugIORQIntNext;

    DebugIORQ _debugIORQEvsMatch[MAX_DEBUG_IORQ_EVS];
    int _debugIORQMatchNum;
    int _debugIORQMatchExpected;
#endif

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Access
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusAccess
{
public:
    BusAccess();

    // Initialise the bus raider
    void init();
    void busAccessReset();

    // Bus Sockets - used to hook things like waitInterrupts, busControl, etc
    int busSocketAdd(bool enabled, BusAccessCBFnType* busAccessCallback,
            BusActionCBFnType* busActionCallback, 
            bool waitOnMemory, bool waitOnIO,
            bool resetPending, uint32_t resetDurationTStates,
            bool nmiPending, uint32_t nmiDurationTStates,
            bool irqPending, uint32_t irqDurationTStates,
            bool busMasterRequest, BR_BUS_ACTION_REASON busMasterReason,
            bool holdInWaitReq, void* pSourceObject);
    void busSocketEnable(int busSocket, bool enable);
    bool busSocketIsEnabled(int busSocket);

    // Wait state enablement
    void waitOnMemory(int busSocket, bool isOn);
    void waitOnIO(int busSocket, bool isOn);
    bool waitIsOnMemory();

    // Min cycle Us when in waitOnMemory mode
    void waitSetCycleUs(uint32_t cycleUs);

    // Reset, NMI and IRQ on target
    void targetReqReset(int busSocket, int durationTStates = -1);
    void targetReqNMI(int busSocket, int durationTStates = -1);
    void targetReqIRQ(int busSocket, int durationTStates = -1);
    void targetReqBus(int busSocket, BR_BUS_ACTION_REASON busMasterReason);
    void targetPageForInjection(int busSocket, bool pageOut);

    // Bus control
    bool isUnderControl();
    
    // Read and write blocks
    BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, bool busRqAndRelease, bool iorq);
    BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pData, uint32_t len, bool busRqAndRelease, bool iorq);

    // Wait hold and release
    void waitRelease();
    bool waitIsHeld();
    void waitHold(int busSocket, bool hold);

    // Suspend bus detail for one cycle - used to fix a PIB contention issue
    void waitSuspendBusDetailOneCycle();

    // Service
    void service();

    // Get return type string
    const char* retcString(BR_RETURN_TYPE retc)
    {
        switch (retc)
        {
            case BR_OK: return "ok";
            case BR_ERR: return "general error";
            case BR_ALREADY_DONE: return "already paused";
            case BR_NO_BUS_ACK: return "BUSACK not received";
            default: return "unknown error";
        }
    }

    // Clock
    void clockSetup();
    void clockSetFreqHz(uint32_t freqHz);
    void clockEnable(bool en);
    uint32_t clockCurFreqHz();
    uint32_t clockGetMinFreqHz();
    uint32_t clockGetMaxFreqHz();

    // Status
    void getStatus(BusAccessStatusInfo& statusInfo);
    void clearStatus();

    // External API low-level bus control
    void rawBusControlEnable(bool en);
    void rawBusControlClearWait();
    void rawBusControlWaitDisable();
    void rawBusControlClockEnable(bool en);
    bool rawBusControlTakeBus();
    void rawBusControlReleaseBus();
    void rawBusControlSetAddress(uint32_t addr);
    void rawBusControlSetData(uint32_t data);
    uint32_t rawBusControlReadRaw();
    uint32_t rawBusControlReadCtrl();
    void rawBusControlReadAll(uint32_t& ctrl, uint32_t& addr, uint32_t& data);
    void rawBusControlSetPin(uint32_t pinNumber, bool level);
    bool rawBusControlGetPin(uint32_t pinNumber);
    uint32_t rawBusControlReadPIB();
    void rawBusControlWritePIB(uint32_t val);
    void rawBusControlMuxSet(uint32_t val);
    void rawBusControlMuxClear();

    // Version
    static const int HW_VERSION_DEFAULT = 20;
    int getHwVersion()
    {
        return _hwVersionNumber;
    }
    void setHwVersion(int hwVersion)
    {
        _hwVersionNumber = hwVersion;
    }

    // Control of bus paging pin
    void busPagePinSetActive(bool active);

    // Debug
    void isrAssert(int code);
    void isrValue(int code, int val);
    void isrPeak(int code, int val);
    int isrAssertGetCount(int code);
    void formatCtrlBus(uint32_t ctrlBus, char* msgBuf, int maxMsgBufLen);

    // Bus request/ack
    void controlRequest();
    BR_RETURN_TYPE controlRequestAndTake();
    void controlRelease();
    void controlTake();
    bool waitForBusAck(bool ack);

private:
    // Hardware version
    // V1.7 ==> 17, V2.0 ==> 20
    int _hwVersionNumber;
    
    // Bus Sockets
    static const int MAX_BUS_SOCKETS = 10;
    BusSocketInfo _busSockets[MAX_BUS_SOCKETS];
    int _busSocketCount;

    // Bus service active
    bool _busServiceEnabled;

    // Current wait state flags
    bool _waitOnMemory;
    bool _waitOnIO;

    // Wait currently asserted
    volatile bool _waitAsserted;

    // Outputs enabled to allow target read
    volatile bool _targetReadInProgress;
    
    // Paging
    volatile bool _targetPageInOnReadComplete;

    // Rate at which wait is released when free-cycling
    volatile uint32_t _waitCycleLengthUs;
    volatile uint32_t _waitAssertedStartUs;

    // Hold in wait state
    volatile bool _waitHold;

    // Suspend bus detail for one cycle
    volatile bool _waitSuspendBusDetailOneCycle;

    // Action requested for next wait and parameter
    volatile int _busActionSocket;
    volatile BR_BUS_ACTION _busActionType;
    volatile uint32_t _busActionInProgressStartUs;
    volatile uint32_t _busActionAssertedStartUs;
    volatile uint32_t _busActionAssertedMaxUs;
    volatile bool _busActionSyncWithWait;
    const int TIMER_ISR_PERIOD_US = 100;

    // Bus access state
    enum BUS_ACTION_STATE
    {
        BUS_ACTION_STATE_NONE,
        BUS_ACTION_STATE_PENDING,
        BUS_ACTION_STATE_ASSERTED
    };
    volatile BUS_ACTION_STATE _busActionState;

    // Bus currently under BusRaider control
    volatile bool _busIsUnderControl;

    // Debug
    volatile int _isrAssertCounts[ISR_ASSERT_NUM_CODES];

    // Clock generator
    TargetClockGenerator _clockGenerator;

    // Status info
    BusAccessStatusInfo _statusInfo; 

private:
    // Bus actions
    void busActionCheck();
    bool busActionHandleStart();
    void busActionHandleActive();
    void busActionClearFlags();
    void busActionCallback(BR_BUS_ACTION busActionType, BR_BUS_ACTION_REASON reason);
    bool busAccessHandleIrqAck();

    // Set address
    void addrLowSet(uint32_t lowAddrByte);
    void addrLowInc();
    void addrHighSet(uint32_t highAddrByte);
    void addrSet(unsigned int addr);

    // Control bus read
    uint32_t controlBusRead();
    void addrAndDataBusRead(uint32_t& addr, uint32_t& dataBusVals);

    // Control the PIB (bus used to transfer data to/from Pi)
    inline void pibSetOut()
    {
        write32(BR_PIB_GPF_REG, (read32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_OUTPUT);
    }

    inline void pibSetIn()
    {
        write32(BR_PIB_GPF_REG, (read32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
    }

    inline void pibSetValue(uint8_t val)
    {
        uint32_t setBits = ((uint32_t)val) << BR_DATA_BUS;
        uint32_t clrBits = (~(((uint32_t)val) << BR_DATA_BUS)) & (~BR_PIB_MASK);
        write32(ARM_GPIO_GPSET0, setBits);
        write32(ARM_GPIO_GPCLR0, clrBits);
    }
 
    // Get a value from the PIB (pins used for data/address bus access)
    inline uint8_t pibGetValue()
    {
        return (read32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
    }

    // Check if bus request has been acknowledged
    inline bool controlBusReqAcknowledged()
    {
        return (read32(ARM_GPIO_GPLEV0) & BR_BUSACK_BAR_MASK) == 0;
    }

    // Check if WAIT asserted
    inline bool controlBusWaitAsserted()
    {
        return (read32(ARM_GPIO_GPLEV0) & BR_WAIT_BAR_MASK) == 0;
    }

    // Interrupt service routine for wait states
    void stepTimerISR(void* pParam);
    void serviceWaitActivity();

    // Pin IO
    void setPinOut(int pinNumber, bool val);
    void setPinIn(int pinNumber);

    // Set the MUX
    inline void muxSet(int muxVal)
    {
        if (_hwVersionNumber == 17)
        {
            // Clear first as this is a safe setting - sets HADDR_SER low
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set bits required
            write32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Disable mux initially
            write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
            // Clear all the mux bits
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set bits required
            write32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
            // Enable the mux
            write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
#else
            // Clear first
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set bits required
            write32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
#endif
        }
    }

    // Clear the MUX
    inline void muxClear()
    {
        if (_hwVersionNumber == 17)
        {
            // Clear to a safe setting - sets HADDR_SER low
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Disable the mux
            write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
            // Clear to a safe setting - sets LADDR_CK low
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#else
            // Clear to a safe setting - sets LADDR_CK low
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
        }
    }

    // Mux set data bus driver output enable
    inline void muxDataBusOutputEnable()
    {
        if (_hwVersionNumber == 17)
        {
            // Clear first as this is a safe setting - sets HADDR_SER low
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set OE bit
            write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Time pulse width
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            // Clear again
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Clear then set the output enable
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Pulse mux enable
            write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
#else
            // Clear then set the output enable
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);       
#endif
        }
    }

    // Mux clear low address
    inline void muxClearLowAddr()
    {
        if (_hwVersionNumber == 17)
        {
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            write32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Time pulse width
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            // Clear again
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Clear then set the low address clear line
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            write32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Pulse mux enable
            write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
            write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
#else
            // Clear then set the low address clear line
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            write32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
            write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
        }
        
    }

    // Set signal (RESET/IRQ/NMI)
    void setSignal(BR_BUS_ACTION busAction, bool assert);

    // Wait control
    void waitSetupMREQAndIORQEnables();
    void waitResetFlipFlops(bool forceClear = false);
    void waitClearDetected();
    void waitHandleNew();
    void waitEnablementUpdate();
    void waitGenerationDisable();
    void waitHandleReadRelease();

    // Paging
    void busAccessCallbackPageIn();

    // Read and write bytes
    void byteWrite(uint32_t byte, int iorq);
    uint8_t byteRead(int iorq);

private:
    // Timeouts
    static const int MAX_WAIT_FOR_PENDING_ACTION_US = 100000;
    static const int MAX_WAIT_FOR_CTRL_LINES_COUNT = 10;
    static const int MAX_WAIT_FOR_CTRL_BUS_VALID_US = 10;
    static const int MIN_LOOP_COUNT_FOR_CTRL_BUS_VALID = 100;

    // Period target write control bus line is asserted during a write
    static const int CYCLES_DELAY_FOR_WRITE_TO_TARGET = 250;

    // Delay for settling of high address lines on PIB read
    static const int CYCLES_DELAY_FOR_HIGH_ADDR_READ = 100;

    // Period target read control bus line is asserted during a read from the PIB (any bus element)
    static const int CYCLES_DELAY_FOR_READ_FROM_PIB = 50;

    // Max wait for end of read cycle
    static const int MAX_WAIT_FOR_END_OF_READ_US = 10;

    // Delay in machine cycles for setting the pulse width when clearing/incrementing the address counter/shift-reg
    static const int CYCLES_DELAY_FOR_CLEAR_LOW_ADDR = 15;
    static const int CYCLES_DELAY_FOR_CLOCK_LOW_ADDR = 15;
    static const int CYCLES_DELAY_FOR_LOW_ADDR_SET = 15;
    static const int CYCLES_DELAY_FOR_HIGH_ADDR_SET = 20;
    static const int CYCLES_DELAY_FOR_WAIT_CLEAR = 50;
    static const int CYCLES_DELAY_FOR_MREQ_FF_RESET = 20;
    static const int CYCLES_DELAY_FOR_DATA_DIRN = 20;
    static const int CYCLES_DELAY_FOR_TARGET_READ = 100;
    static const int CYCLES_DELAY_FOR_OUT_FF_SET = 10;

    // Delay in machine cycles for M1 to settle
    static const int CYCLES_DELAY_FOR_M1_SETTLING = 100;
};

