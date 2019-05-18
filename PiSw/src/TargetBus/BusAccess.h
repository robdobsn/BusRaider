// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "TargetCPU.h"
#include "../System/BCM2835.h"
#include "../System/lowlib.h"

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
#define BR_MUX_HADDR_SER_LOW 0x00
#define BR_MUX_LADDR_CLR_BAR_LOW 0x04
#define BR_MUX_DATA_OE_BAR_LOW 0x01
#define BR_MUX_RESET_Z80_BAR_LOW 0x05
#define BR_MUX_IRQ_BAR_LOW 0x02
#define BR_MUX_NMI_BAR_LOW 0x06
#define BR_MUX_LADDR_OE_BAR 0x03
#define BR_MUX_HADDR_OE_BAR 0x07

// The following is used when HADDR_SER needs to be HIGH - the default is low
// It actually sets MUX to BR_MUX_HADDR_OE_BAR - this is ok as PIB is input
// at this stage
#define BR_MUX_HADDR_SER_HIGH BR_MUX_HADDR_OE_BAR 

// Pi pins used for control of host bus
#define BR_BUSRQ_BAR 19 // SPI1 MISO
#define BR_BUSACK_BAR 2 // SDA
#define BR_MUX_0 11 // SPI0 SCLK
#define BR_MUX_1 9 // SPI0 MISO
#define BR_MUX_2 10 // SPI0 MOSI
#define BR_IORQ_WAIT_EN 12 // GPIO12
#define BR_MREQ_WAIT_EN 13 // GPIO13
#define BR_WR_BAR 17 // SPI1 CE1
#define BR_RD_BAR 18 // SPI1 CE0
#define BR_MREQ_BAR 0 // ID_SD
#define BR_IORQ_BAR 1 // ID_SC
#define BR_DATA_BUS 20 // GPIO20..27
#define BR_M1_PIB_BAR 20 // Piggy backing on the PIB (with resistor to avoid conflict)
#define BR_PUSH_ADDR_BAR 3 // SCL
#define BR_HADDR_CK 7 // SPI0 CE1
#define BR_LADDR_CK 16 // SPI1 CE2
#define BR_DATA_DIR_IN 6
#define BR_WAIT_BAR_PIN 5 // GPIO5
#define BR_CLOCK_PIN 4 // GPIO4
#define BR_PAGING_RAM_PIN 8 // CPI CE0

// Masks for above
#define BR_IORQ_WAIT_EN_MASK (1 << BR_IORQ_WAIT_EN)
#define BR_MREQ_WAIT_EN_MASK (1 << BR_MREQ_WAIT_EN)
#define BR_MREQ_BAR_MASK (1 << BR_MREQ_BAR)
#define BR_IORQ_BAR_MASK (1 << BR_IORQ_BAR)
#define BR_WR_BAR_MASK (1 << BR_WR_BAR)
#define BR_RD_BAR_MASK (1 << BR_RD_BAR)
#define BR_BUSACK_BAR_MASK (1 << BR_BUSACK_BAR)
#define BR_WAIT_BAR_MASK (1 << BR_WAIT_BAR_PIN)
#define BR_M1_PIB_BAR_MASK (1 << BR_M1_PIB_BAR)
#define BR_ANY_EDGE_MASK 0xffffffff

// M1 piggy back onto PIB line
#define BR_M1_PIB_DATA_LINE 0

// Pi debug pin
#define BR_DEBUG_PI_SPI0_CE0 8 // SPI0 CE0

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

// Width of a reset, NMI, IRQ pulses
#define BR_RESET_PULSE_US 1000
#define BR_NMI_PULSE_US 50
#define BR_IRQ_PULSE_US 50
#define BR_BUSRQ_MAX_US 100

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
    static const int MAX_WAIT_FOR_BUSACK_US = 100;

    // Socket enablement
    bool enabled;

    // Callbacks
    BusAccessCBFnType* busAccessCallback;
    BusActionCBFnType* busActionCallback;

    // Flags
    bool waitOnMemory;
    bool waitOnIO;

    // Bus action and duration
    volatile BR_BUS_ACTION busActionRequested;
    volatile uint32_t busActionDurationUs;

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
        return busActionRequested;
    }

    void clear(BR_BUS_ACTION type)
    {
        if (type == BR_BUS_ACTION_BUSRQ)
            busMasterRequest = false;
        else
            busActionRequested = BR_BUS_ACTION_NONE;
    }

    uint32_t getAssertUs()
    {
        if (busMasterRequest)
            return MAX_WAIT_FOR_BUSACK_US;
        return busActionDurationUs;
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
    static void targetReqReset(int busSocket, int durationUs = -1);
    static void targetReqNMI(int busSocket, int durationUs = -1);
    static void targetReqIRQ(int busSocket, int durationUs = -1);
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

    // Debug
    static void isrAssert(int code);
    static void isrValue(int code, int val);
    static void isrPeak(int code, int val);
    static int isrAssertGetCount(int code);

private:
    // Bus Sockets
    static const int MAX_BUS_SOCKETS = 10;
    static BusSocketInfo _busSockets[MAX_BUS_SOCKETS];
    static int _busSocketCount;

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
    static volatile bool _busActionInProgress;
    static volatile bool _busActionAsserted;
    static volatile bool _busActionSyncWithWait;
    static const int TIMER_ISR_PERIOD_US = 100;

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
    static bool busActionAssertStart();
    static void busActionAssertActive();
    static void busActionClearFlags();
    static void busActionCallback(BR_BUS_ACTION busActionType, BR_BUS_ACTION_REASON reason);
    // static BR_BUS_ACTION busActionCheckNext(bool initiateAction);

    // Set address
    static void addrLowSet(uint32_t lowAddrByte);
    static void addrLowInc();
    static void addrHighSet(uint32_t highAddrByte);
    static void addrSet(unsigned int addr);

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

    // Bus request/ack
    static void controlRequest();
    static BR_RETURN_TYPE controlRequestAndTake();
    static void controlRelease();
    static int controlBusAcknowledged();
    static void controlTake();

    // Interrupt service routine for wait states
    static void stepTimerISR(void* pParam);
    static void serviceWaitActivity();

    // Pin IO
    static void setPinOut(int pinNumber, bool val);
    static void setPinIn(int pinNumber);

    // Set the MUX
    static inline void muxSet(int muxVal)
    {
        // Clear first as this is a safe setting - sets HADDR_SER low
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
        // Now set bits required
        WR32(ARM_GPIO_GPSET0, muxVal << BR_MUX_LOW_BIT_POS);
    }

    // Clear the MUX
    static inline void muxClear()
    {
        // Clear to a safe setting - sets HADDR_SER low
        WR32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    }

    // Set signal (RESET/IRQ/NMI)
    static void setSignal(BR_BUS_ACTION busAction, bool assert);

    // Wait control
    static void waitSetupMREQAndIORQEnables();
    static void waitResetFlipFlops();
    static void waitClearDetected();
    static void waitHandleNew();
    static void waitEnablementUpdate();
    static void waitGenerationDisable();
    static void waitHandleReadRelease();

    // Paging
    static void pagingPageIn();

    // Read and write bytes
    static void byteWrite(uint32_t byte, int iorq);
    static uint8_t byteRead(int iorq);

private:
    // Timeouts
    static const int MAX_WAIT_FOR_PENDING_ACTION_US = 2000;
    static const int MAX_WAIT_FOR_CTRL_LINES_COUNT = 10;

    // Period target write control bus line is asserted during a write
    static const int CYCLES_DELAY_FOR_WRITE_TO_TARGET = 250;

    // Delay for settling of high address lines on PIB read
    static const int CYCLES_DELAY_FOR_HIGH_ADDR_READ = 100;

    // Period target read control bus line is asserted during a read from the PIB (any bus element)
    static const int CYCLES_DELAY_FOR_READ_FROM_PIB = 25;

    // Max wait for end of read cycle
    static const int MAX_WAIT_FOR_END_OF_READ_US = 10;

    // Delay in machine cycles for setting the pulse width when clearing/incrementing the address counter/shift-reg
    static const int CYCLES_DELAY_FOR_CLEAR_LOW_ADDR = 20;
    static const int CYCLES_DELAY_FOR_LOW_ADDR_SET = 20;
    static const int CYCLES_DELAY_FOR_HIGH_ADDR_SET = 20;
    static const int CYCLES_DELAY_FOR_WAIT_CLEAR = 50;
    static const int CYCLES_DELAY_FOR_MREQ_FF_RESET = 20;
    static const int CYCLES_DELAY_FOR_DATA_DIRN = 20;
    static const int CYCLES_DELAY_FOR_TARGET_READ = 100;
    static const int CYCLES_DELAY_FOR_OUT_FF_SET = 10;

    // Delay in machine cycles for M1 to settle
    static const int CYCLES_DELAY_FOR_M1_SETTLING = 100;
};

