// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetCPU.h"
#include "../System/BCM2835.h"
#include "../System/lowlib.h"

// #define ISR_TEST 1

#define V2_PROTO_USING_MUX_EN 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defs
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Multiplexer
// Wiring is a little off as A0, A1, A2 on 74HC138 are Pi 11, 9, 10 respectively
// Pi GPIO0    74HC138      CTRL Value
// 11 10 09    A2 A1 A0     
//  0  0  0     0  0  0     PI_HADDR_SER
//  0  0  1     0  1  0     PI_DATA_OE_BAR
//  0  1  0     1  0  0     PI_IRQ_BAR
//  0  1  1     1  1  0     PI_LADDR_OE_BAR
//  1  0  0     0  0  1     PI_LADDR_CLR_BAR
//  1  0  1     0  1  1     PI_RESET_Z80_BAR
//  1  1  0     1  0  1     PI_NMI_BAR
//  1  1  1     1  1  1     PI_HADDR_OE_BAR
#define BR_MUX_LOW_BIT_POS 9
#define BR_MUX_CTRL_BIT_MASK (0x07 << BR_MUX_LOW_BIT_POS)
#define BR_MUX_LADDR_CLK 0x00
#define BR_MUX_LADDR_CLR_BAR_LOW 0x04
#define BR_MUX_DATA_OE_BAR_LOW 0x01
#define BR_MUX_RESET_Z80_BAR_LOW 0x05
#define BR_MUX_IRQ_BAR_LOW 0x02
#define BR_MUX_NMI_BAR_LOW 0x06
#define BR_MUX_LADDR_OE_BAR 0x03
#define BR_MUX_HADDR_OE_BAR 0x07

// Pi pins used for control of host bus
#define BR_BUSRQ_BAR 19 // SPI1 MISO
#define BR_BUSACK_BAR 2 // SDA
#define BR_MUX_0 11 // SPI0 SCLK
#define BR_MUX_1 9 // SPI0 MISO
#define BR_MUX_2 10 // SPI0 MOSI
#define BR_MUX_EN_BAR 16 // SPI1 CE2
#define BR_IORQ_WAIT_EN 12 // GPIO12
#define BR_MREQ_WAIT_EN 13 // GPIO13
#define BR_WR_BAR 17 // SPI1 CE1
#define BR_RD_BAR 18 // SPI1 CE0
#define BR_MREQ_BAR 0 // ID_SD
#define BR_IORQ_BAR 1 // ID_SC
#define BR_DATA_BUS 20 // GPIO20..27
#define BR_HADDR_CK 7 // SPI0 CE1
#define BR_DATA_DIR_IN 6
#define BR_WAIT_BAR_PIN 5 // GPIO5
#define BR_CLOCK_PIN 4 // GPIO4
#define BR_PAGING_RAM_PIN 8 // CPI CE0

// V1.7 hardware pins
#define BR_V17_LADDR_CK 16 // SPI1 CE2
#define BR_V17_LADDR_CK_MASK (1 << BR_V17_LADDR_CK)
#define BR_V17_M1_PIB_BAR 20 // Piggy backing on the PIB (with resistor to avoid conflict)
#define BR_V17_PUSH_ADDR_BAR 3 // SCL
#define BR_V17_M1_PIB_BAR_MASK (1 << BR_V17_M1_PIB_BAR)
#define BR_V17_M1_PIB_DATA_LINE 0
#define BR_V17_MUX_HADDR_SER_LOW 0x00
#define BR_V17_MUX_HADDR_SER_HIGH BR_MUX_LADDR_CLR_BAR_LOW 

// V2.0 hardware pins
#define BR_V20_M1_BAR 3 // SCL
#define BR_V20_M1_BAR_MASK (1 << BR_V20_M1_BAR)

// Masks for above
#define BR_BUSRQ_BAR_MASK (1 << BR_BUSRQ_BAR)
#define BR_MUX_EN_BAR_MASK (1 << BR_MUX_EN_BAR)
#define BR_IORQ_WAIT_EN_MASK (1 << BR_IORQ_WAIT_EN)
#define BR_MREQ_WAIT_EN_MASK (1 << BR_MREQ_WAIT_EN)
#define BR_MREQ_BAR_MASK (1 << BR_MREQ_BAR)
#define BR_IORQ_BAR_MASK (1 << BR_IORQ_BAR)
#define BR_WR_BAR_MASK (1 << BR_WR_BAR)
#define BR_RD_BAR_MASK (1 << BR_RD_BAR)
#define BR_BUSACK_BAR_MASK (1 << BR_BUSACK_BAR)
#define BR_WAIT_BAR_MASK (1 << BR_WAIT_BAR_PIN)
#define BR_DATA_DIR_IN_MASK (1 << BR_DATA_DIR_IN)
#define BR_ANY_EDGE_MASK 0xffffffff

// Pi debug pin
#define BR_DEBUG_PI_SPI0_CE0 8 // SPI0 CE0
#define BR_DEBUG_PI_SPI0_CE0_MASK (1 << BR_DEBUG_PI_SPI0_CE0) // SPI0 CE0

// Direct access to Pi PIB (used for data transfer to/from host data bus)
#define BR_PIB_MASK (~((uint32_t)0xff << BR_DATA_BUS))
#define BR_PIB_GPF_REG ARM_GPIO_GPFSEL2
#define BR_PIB_GPF_MASK 0xff000000
#define BR_PIB_GPF_INPUT 0x00000000
#define BR_PIB_GPF_OUTPUT 0x00249249

// Debug code for ISR
#define ISR_ASSERT(code) BusAccess::isrAssert(code)
#define ISR_VALUE(code, val) BusAccess::isrValue(code, val)
#define ISR_PEAK(code, val) BusAccess::isrPeak(code, val)
#define ISR_ASSERT_CODE_NONE 0
#define ISR_ASSERT_CODE_DEBUG_A 1
#define ISR_ASSERT_CODE_DEBUG_B 2
#define ISR_ASSERT_CODE_DEBUG_C 3
#define ISR_ASSERT_CODE_DEBUG_D 4
#define ISR_ASSERT_CODE_DEBUG_E 5
#define ISR_ASSERT_CODE_DEBUG_F 6
#define ISR_ASSERT_CODE_DEBUG_G 7
#define ISR_ASSERT_CODE_DEBUG_H 8
#define ISR_ASSERT_CODE_DEBUG_I 9
#define ISR_ASSERT_CODE_DEBUG_J 10
#define ISR_ASSERT_CODE_DEBUG_K 11
#define ISR_ASSERT_NUM_CODES 12

// Timing of a reset, NMI, IRQ and wait for BUSACK
#define BR_RESET_PULSE_T_STATES 100
#define BR_NMI_PULSE_T_STATES 32
#define BR_IRQ_PULSE_T_STATES 32
#define BR_MAX_WAIT_FOR_BUSACK_T_STATES 1000

// Max time bound in service function
#define BR_MAX_TIME_IN_SERVICE_LOOP_US 10000
#define BR_MAX_TIME_IN_READ_LOOP_US 10000

// Clock frequency for debug
#define BR_TARGET_DEBUG_CLOCK_HZ 500000

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback types
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Callback types
typedef void BusAccessCBFnType(uint32_t addr, uint32_t data, uint32_t flags, uint32_t& curRetVal);
typedef void BusActionCBFnType(BR_BUS_ACTION actionType, BR_BUS_ACTION_REASON reason);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bus Socket Info - this is used to plug-in to the BusAccess layer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusSocketInfo
{
public:
    // Socket enablement
    bool enabled;

    // Callbacks
    BusAccessCBFnType* busAccessCallback;
    BusActionCBFnType* busActionCallback;

    // Flags
    bool waitOnMemory;
    bool waitOnIO;

    // Bus actions and duration
    volatile bool resetPending;
    volatile uint32_t resetDurationTStates;
    volatile bool nmiPending;
    volatile uint32_t nmiDurationTStates;
    volatile bool irqPending;
    volatile uint32_t irqDurationTStates;

    // Bus master request and reason
    volatile bool busMasterRequest;
    volatile BR_BUS_ACTION_REASON busMasterReason;

    // Bus hold in wait
    volatile bool holdInWaitReq;

    // Get type of bus action
    BR_BUS_ACTION getType()
    {
        if (busMasterRequest)
            return BR_BUS_ACTION_BUSRQ;
        if (resetPending)
            return BR_BUS_ACTION_RESET;
        if (nmiPending)
            return BR_BUS_ACTION_NMI;
        if (irqPending)
            return BR_BUS_ACTION_IRQ;
        return BR_BUS_ACTION_NONE;
    }

    void clearDown(BR_BUS_ACTION type)
    {
        if (type == BR_BUS_ACTION_BUSRQ)
            busMasterRequest = false;
        else if (type == BR_BUS_ACTION_RESET)
            resetPending = false;
        else if (type == BR_BUS_ACTION_NMI)
            nmiPending = false;
        else if (type == BR_BUS_ACTION_IRQ)
            irqPending = false;
    }

    // Time calc
    static uint32_t getUsFromTStates(uint32_t tStates, uint32_t clockFreqHz, uint32_t defaultTStates = 1)
    {
        uint32_t tS = tStates >= 1 ? tStates : defaultTStates;
        uint32_t uS = 1000000 * tS / clockFreqHz;
        if (uS <= 0)
            uS = 1;
        return uS;
    }

    uint32_t getAssertUs(BR_BUS_ACTION type, uint32_t clockFreqHz)
    {
        if (type == BR_BUS_ACTION_BUSRQ)
            return getUsFromTStates(BR_MAX_WAIT_FOR_BUSACK_T_STATES, clockFreqHz);
        else if (type == BR_BUS_ACTION_RESET)
            return getUsFromTStates(resetDurationTStates, clockFreqHz, BR_RESET_PULSE_T_STATES);
        else if (type == BR_BUS_ACTION_NMI)
            return getUsFromTStates(nmiDurationTStates, clockFreqHz, BR_NMI_PULSE_T_STATES);
        else if (type == BR_BUS_ACTION_IRQ)
            return getUsFromTStates(irqDurationTStates, clockFreqHz, BR_IRQ_PULSE_T_STATES);
        return 0;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

class TargetClockGenerator;

class BusAccess
{
public:
    // Initialise the bus raider
    static void init();
    static void busAccessReset();

    // Bus Sockets - used to hook things like waitInterrupts, busControl, etc
    static int busSocketAdd(BusSocketInfo& busSocketInfo);
    static void busSocketEnable(int busSocket, bool enable);
    static bool busSocketIsEnabled(int busSocket);

    // Wait state enablement
    static void waitOnMemory(int busSocket, bool isOn);
    static void waitOnIO(int busSocket, bool isOn);
    static bool waitIsOnMemory();

    // Min cycle Us when in waitOnMemory mode
    static void waitSetCycleUs(uint32_t cycleUs);

    // Reset, NMI and IRQ on target
    static void targetReqReset(int busSocket, int durationTStates = -1);
    static void targetReqNMI(int busSocket, int durationTStates = -1);
    static void targetReqIRQ(int busSocket, int durationTStates = -1);
    static void targetReqBus(int busSocket, BR_BUS_ACTION_REASON busMasterReason);
    static void targetPageForInjection(int busSocket, bool pageOut);

    // Bus control
    static bool isUnderControl();
    
    // Read and write blocks
    static BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, bool busRqAndRelease, bool iorq);
    static BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pData, uint32_t len, bool busRqAndRelease, bool iorq);

    // Wait hold and release
    static void waitRelease();
    static bool waitIsHeld();
    static void waitHold(int busSocket, bool hold);

    // Suspend bus detail for one cycle - used to fix a PIB contention issue
    static void waitSuspendBusDetailOneCycle();

    // Service
    static void service();

    // Get return type string
    static const char* retcString(BR_RETURN_TYPE retc)
    {
        switch (retc)
        {
            case BR_OK: return "ok";
            case BR_ERR: return "err";
            case BR_ALREADY_DONE: return "already paused";
            case BR_NO_BUS_ACK: return "bus ack not received";
            default: return "unknown";
        }
    }

    // Clock
    static void clockSetup();
    static void clockSetFreqHz(uint32_t freqHz);
    static void clockEnable(bool en);
    static uint32_t clockCurFreqHz();
    static uint32_t clockGetMinFreqHz();
    static uint32_t clockGetMaxFreqHz();

    // Status
    static void getStatus(BusAccessStatusInfo& statusInfo);
    static void clearStatus();

    // External API low-level bus control
    static void rawBusControlEnable(bool en);
    static void rawBusControlClearWait();
    static void rawBusControlWaitDisable();
    static void rawBusControlClockEnable(bool en);
    static bool rawBusControlTakeBus();
    static void rawBusControlReleaseBus();
    static void rawBusControlSetAddress(uint32_t addr);
    static void rawBusControlSetData(uint32_t data);
    static uint32_t rawBusControlReadRaw();
    static void rawBusControlSetPin(uint32_t pinNumber, bool level);
    static bool rawBusControlGetPin(uint32_t pinNumber);
    static uint32_t rawBusControlReadPIB();
    static void rawBusControlWritePIB(uint32_t val);
    static void rawBusControlMuxSet(uint32_t val);
    static void rawBusControlMuxClear();

    // Version
    static const int HW_VERSION_DEFAULT = 20;
    static int getHwVersion()
    {
        return _hwVersionNumber;
    }
    static void setHwVersion(int hwVersion)
    {
        _hwVersionNumber = hwVersion;
    }

    // Control of bus paging pin
    static void busPagePinSetActive(bool active);

    // Debug
    static void isrAssert(int code);
    static void isrValue(int code, int val);
    static void isrPeak(int code, int val);
    static int isrAssertGetCount(int code);

    // Bus request/ack
    static void controlRequest();
    static BR_RETURN_TYPE controlRequestAndTake();
    static void controlRelease();
    static void controlTake();
    static bool waitForBusAck(bool ack);

private:
    // Hardware version
    // V1.7 ==> 17, V2.0 ==> 20
    static int _hwVersionNumber;
    
    // Bus Sockets
    static const int MAX_BUS_SOCKETS = 10;
    static BusSocketInfo _busSockets[MAX_BUS_SOCKETS];
    static int _busSocketCount;

    // Bus service active
    static bool _busServiceEnabled;

    // Current wait state flags
    static bool _waitOnMemory;
    static bool _waitOnIO;

    // Wait currently asserted
    static volatile bool _waitAsserted;

    // Outputs enabled to allow target read
    static volatile bool _targetReadInProgress;

    // Paging
    static volatile bool _targetPageInOnReadComplete;

    // Rate at which wait is released when free-cycling
    static volatile uint32_t _waitCycleLengthUs;
    static volatile uint32_t _waitAssertedStartUs;

    // Hold in wait state
    static volatile bool _waitHold;

    // Suspend bus detail for one cycle
    static volatile bool _waitSuspendBusDetailOneCycle;

    // Action requested for next wait and parameter
    static volatile int _busActionSocket;
    static volatile BR_BUS_ACTION _busActionType;
    static volatile uint32_t _busActionInProgressStartUs;
    static volatile uint32_t _busActionAssertedStartUs;
    static volatile uint32_t _busActionAssertedMaxUs;
    static volatile bool _busActionSyncWithWait;
    static const int TIMER_ISR_PERIOD_US = 100;

    // Bus access state
    enum BUS_ACTION_STATE
    {
        BUS_ACTION_STATE_NONE,
        BUS_ACTION_STATE_PENDING,
        BUS_ACTION_STATE_ASSERTED
    };
    static volatile BUS_ACTION_STATE _busActionState;

    // Bus currently under BusRaider control
    static volatile bool _busIsUnderControl;

    // Debug
    static volatile int _isrAssertCounts[ISR_ASSERT_NUM_CODES];

    // Clock generator
    static TargetClockGenerator _clockGenerator;

    // Status info
    static BusAccessStatusInfo _statusInfo; 

private:
    // Bus actions
    static void busActionCheck();
    static bool busActionHandleStart();
    static void busActionHandleActive();
    static void busActionClearFlags();
    static void busActionCallback(BR_BUS_ACTION busActionType, BR_BUS_ACTION_REASON reason);
    static bool busAccessHandleIrqAck();

    // Set address
    static void addrLowSet(uint32_t lowAddrByte);
    static void addrLowInc();
    static void addrHighSet(uint32_t highAddrByte);
    static void addrSet(unsigned int addr);

    // Control bus read
    static uint32_t controlBusRead();

    // Control the PIB (bus used to transfer data to/from Pi)
    static inline void pibSetOut()
    {
        WR32(BR_PIB_GPF_REG, (RD32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_OUTPUT);
    }

    static inline void pibSetIn()
    {
        WR32(BR_PIB_GPF_REG, (RD32(BR_PIB_GPF_REG) & BR_PIB_GPF_MASK) | BR_PIB_GPF_INPUT);
    }

    static inline void pibSetValue(uint8_t val)
    {
        uint32_t setBits = ((uint32_t)val) << BR_DATA_BUS;
        uint32_t clrBits = (~(((uint32_t)val) << BR_DATA_BUS)) & (~BR_PIB_MASK);
        WR32(ARM_GPIO_GPSET0, setBits);
        WR32(ARM_GPIO_GPCLR0, clrBits);
    }
 
    // Get a value from the PIB (pins used for data/address bus access)
    static inline uint8_t pibGetValue()
    {
        return (RD32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
    }

    // Check if bus request has been acknowledged
    static inline bool controlBusReqAcknowledged()
    {
        return (RD32(ARM_GPIO_GPLEV0) & BR_BUSACK_BAR_MASK) == 0;
    }

    // Check if WAIT asserted
    static inline bool controlBusWaitAsserted()
    {
        return (RD32(ARM_GPIO_GPLEV0) & BR_WAIT_BAR_MASK) == 0;
    }

    // Interrupt service routine for wait states
    static void stepTimerISR(void* pParam);
    static void serviceWaitActivity();

    // Pin IO
    static void setPinOut(int pinNumber, bool val);
    static void setPinIn(int pinNumber);

    // Set the MUX
    static inline void muxSet(int muxVal)
    {
        if (_hwVersionNumber == 17)
        {
            // Clear first as this is a safe setting - sets HADDR_SER low
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set bits required
            WR32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Clear first
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set bits required
            WR32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
            // Enable the mux
            WR32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
#else
            // Clear first
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set bits required
            WR32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
#endif
        }
    }

    // Clear the MUX
    static inline void muxClear()
    {
        if (_hwVersionNumber == 17)
        {
            // Clear to a safe setting - sets HADDR_SER low
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Disable the mux
            WR32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
            // Clear to a safe setting - sets LADDR_CK low
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#else
            // Clear to a safe setting - sets LADDR_CK low
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
        }
    }

    // Mux set data bus driver output enable
    static inline void muxDataBusOutputEnable()
    {
        if (_hwVersionNumber == 17)
        {
            // Clear first as this is a safe setting - sets HADDR_SER low
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            // Now set OE bit
            WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Time pulse width
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            // Clear again
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Clear then set the output enable
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Pulse mux enable
            WR32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            WR32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
#else
            // Clear then set the output enable
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            WR32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);       
#endif
        }
    }

    // Mux clear low address
    static inline void muxClearLowAddr()
    {
        if (_hwVersionNumber == 17)
        {
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            WR32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Time pulse width
            lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
            // Clear again
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        }
        else
        {
#ifdef V2_PROTO_USING_MUX_EN
            // Clear then set the low address clear line
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            WR32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
            // Pulse mux enable
            WR32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
            WR32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
#else
            // Clear then set the low address clear line
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
            WR32(ARM_GPIO_GPSET0, BR_MUX_LADDR_CLR_BAR_LOW << BR_MUX_LOW_BIT_POS);
            lowlev_cycleDelay(CYCLES_DELAY_FOR_CLEAR_LOW_ADDR);
            WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
#endif
        }
        
    }

    // Set signal (RESET/IRQ/NMI)
    static void setSignal(BR_BUS_ACTION busAction, bool assert);

    // Wait control
    static void waitSetupMREQAndIORQEnables();
    static void waitResetFlipFlops(bool forceClear = false);
    static void waitClearDetected();
    static void waitHandleNew();
    static void waitEnablementUpdate();
    static void waitGenerationDisable();
    static void waitHandleReadRelease();

    // Paging
    static void busAccessCallbackPageIn();

    // Read and write bytes
    static void byteWrite(uint32_t byte, int iorq);
    static uint8_t byteRead(int iorq);

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

