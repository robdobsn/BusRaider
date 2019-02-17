// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint32_t BusAccessCBFnType(uint32_t addr, uint32_t data, uint32_t flags);

// Return codes
typedef enum {
    BR_OK = 0,
    BR_ERR = 1,
    BR_NO_BUS_ACK = 2,
} BR_RETURN_TYPE;

#define BR_MEM_ACCESS_RSLT_NOT_DECODED 0x8000
#define BR_MEM_ACCESS_INSTR_INJECT 0x4000

// Control bus bits used to pass to machines, etc
#define BR_CTRL_BUS_RD 0
#define BR_CTRL_BUS_WR 1
#define BR_CTRL_BUS_MREQ 2
#define BR_CTRL_BUS_IORQ 3
#define BR_CTRL_BUS_M1 4
#define BR_CTRL_BUS_WAIT 5

// Control bit masks
#define BR_CTRL_BUS_RD_MASK (1 << BR_CTRL_BUS_RD)
#define BR_CTRL_BUS_WR_MASK (1 << BR_CTRL_BUS_WR)
#define BR_CTRL_BUS_MREQ_MASK (1 << BR_CTRL_BUS_MREQ)
#define BR_CTRL_BUS_IORQ_MASK (1 << BR_CTRL_BUS_IORQ)
#define BR_CTRL_BUS_M1_MASK (1 << BR_CTRL_BUS_M1)
#define BR_CTRL_BUS_WAIT_MASK (1 << BR_CTRL_BUS_WAIT)

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
#define BR_WAIT_BAR 5 // GPIO5
#define BR_CLOCK_PIN 4 // GPIO4

// Masks for above
#define BR_IORQ_WAIT_EN_MASK (1 << BR_IORQ_WAIT_EN)
#define BR_MREQ_WAIT_EN_MASK (1 << BR_MREQ_WAIT_EN)

// M1 piggy back onto PIB line
#define BR_M1_PIB_DATA_LINE 0

// Pi debug pin
#define BR_DEBUG_PI_SPI0_CE0 8 // SPI0 CE0

// Direct access to Pi PIB (used for data transfer to/from host data bus)
#define BR_PIB_MASK (~((uint32_t)0xff << BR_DATA_BUS))
#define BR_PIB_GPF_REG GPFSEL2
#define BR_PIB_GPF_MASK 0xff000000
#define BR_PIB_GPF_INPUT 0x00000000
#define BR_PIB_GPF_OUTPUT 0x00249249

class BusAccess
{
public:
    // Initialise the bus raider
    static void init();

    // Reset host
    static void targetReset();
    
    // Hold host in reset state - call targetReset() to clear reset
    static void targetResetHold();
    
    // NMI host
    static void targetNMI(int durationUs = -1);

    // IRQ host
    static void targetIRQ(int durationUs = -1);
        
    // Bus control
    static BR_RETURN_TYPE controlRequestAndTake();
    static void controlRelease(bool resetTargetOnRelease);
    static bool isUnderControl();
    
    // Read and write bytes
    static void byteWrite(uint32_t byte, int iorq);
    static uint8_t byteRead(int iorq);

    // Read and write blocks
    static BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq);
    static BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq);
    
    // Enable WAIT
    static void waitEnable(bool enWaitOnIORQ, bool enWaitOnMREQ);

    // Wait interrupt control
    static void waitIntClear();
    static void waitIntDisable();
    static void waitIntEnable();

    // Clear IO
    static void clearAllIO();

    // Service
    static void service();

    // Set and remove callbacks on bus access
    static void accessCallbackAdd(BusAccessCBFnType* pBusAccessCallback);
    static void accessCallbackRemove();

    // Single step
    static bool pause();
    static bool pauseStep();
    static void pauseGetCurrent(uint32_t* pAddr, uint32_t* pData, uint32_t* pFlags);
    static bool pauseIsPaused();
    static bool pauseRelease();

private:
    // Callback on bus access
    static BusAccessCBFnType* _pBusAccessCallback;

    // Current wait state mask for restoring wait enablement after change
    static uint32_t _waitStateEnMask;

    // Wait interrupt enablement cache (so it can be restored after disable)
    static bool _waitIntEnabled;

    // Currently paused - i.e. wait is active
    static volatile bool _pauseIsPaused;

    // Bus currently under BusRaider control
    static volatile bool _busIsUnderControl;

    // Request bus when single stepping
    static bool _requestBusWhenSingleStepping;

    // Bus values while single stepping
    static uint32_t _pauseCurAddr;
    static uint32_t _pauseCurData;
    static uint32_t _pauseCurControlBus;

private:
    // Set address
    static void addrLowSet(uint32_t lowAddrByte);
    static void addrLowInc();
    static void addrHighSet(uint32_t highAddrByte);
    static void addrSet(unsigned int addr);

    // Control the PIB (bus used to transfer data to/from Pi)
    static void pibSetOut();
    static void pibSetIn();
    static void pibSetValue(uint8_t val);
    static uint8_t pibGetValue();

    // Bus request/ack
    static void controlRequest();
    static int controlBusAcknowledged();
    static void controlTake();

    // Interrupt service routine for wait states
    static void waitStateISR(void* pData);

    // Pin IO
    static void setPinOut(int pinNumber, bool val);
    static void setPinIn(int pinNumber);
    // Set the MUX
    static void muxSet(int muxVal);
    // Clear the MUX
    static void muxClear();

    // Wait control
    static void clearWaitFF();
    static void clearWaitDetected();

private:
    // Timeouts
    static const int MAX_WAIT_FOR_ACK_US = 250;
    static const int MAX_WAIT_FOR_CTRL_LINES_COUNT = 10000;

    // Period target write control bus line is asserted during a write
    static const int CYCLES_DELAY_FOR_WRITE_TO_TARGET = 25;

    // Period target read control bus line is asserted during a read from the PIB (any bus element)
    static const int CYCLES_DELAY_FOR_READ_FROM_PIB = 25;

    // Delay in machine cycles for setting the pulse width when clearing/incrementing the address counter/shift-reg
    static const int CYCLES_DELAY_FOR_CLEAR_LOW_ADDR = 20;
    static const int CYCLES_DELAY_FOR_LOW_ADDR_SET = 20;
    static const int CYCLES_DELAY_FOR_HIGH_ADDR_SET = 20;
    static const int CYCLES_DELAY_FOR_WAIT_CLEAR = 20;
    static const int CYCLES_DELAY_FOR_MREQ_FF_RESET = 20;
    static const int CYCLES_DELAY_FOR_DATA_DIRN = 20;

    // Delay in machine cycles for M1 to settle
    static const int CYCLES_DELAY_FOR_M1_SETTLING = 1000;
};
