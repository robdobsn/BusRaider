// Bus Raider
// Rob Dobson 2018

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <circle/gpiopin.h>

typedef uint32_t BusAccessCBFnType(uint32_t addr, uint32_t data, uint32_t flags);

// Return codes
typedef enum {
    BR_OK = 0,
    BR_ERR = 1,
    BR_NO_BUS_ACK = 2,
} BR_RETURN_TYPE;

#define BR_MEM_ACCESS_RSLT_NOT_DECODED 0x8000
#define BR_MEM_ACCESS_RSLT_REQ_PAUSE 0x4000

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
#define BR_M1_PIB_BAR 20 // Piggy backing on the PIB (with 10K resistor to avoid conflict)
#define BR_PUSH_ADDR_BAR 3 // SCL
#define BR_HADDR_CK 7 // SPI0 CE1
#define BR_LADDR_CK 16 // SPI1 CE2
#define BR_DATA_DIR_IN 6
#define BR_WAIT_BAR 5 // GPIO5
#define BR_CLOCK_PIN 4 // GPIO4

// Masks for above
#define BR_IORQ_WAIT_EN_MASK (1 << BR_IORQ_WAIT_EN)
#define BR_MREQ_WAIT_EN_MASK (1 << BR_MREQ_WAIT_EN)

// Pi debug pin
#define BR_DEBUG_PI_SPI0_CE0 8 // SPI0 CE0

// Direct access to Pi PIB (used for data transfer to/from host data bus)
#define BR_PIB_MASK (~((uint32_t)0xff << BR_DATA_BUS))
#define BR_PIB_GPF_REG (ARM_GPIO_BASE + 0x08)
#define BR_PIB_GPF_MASK 0xff000000
#define BR_PIB_GPF_INPUT 0x00000000
#define BR_PIB_GPF_OUTPUT 0x00249249

class BusAccess
{
public:
    BusAccess();
    ~BusAccess();

    // Initialise the bus raider
    void init();    

    // Reset host
    void hostReset();

    // Hold host in reset state - call br_reset_host() to clear reset
    void hostResetHold();

    // NMI host
    void hostNMI();

    // IRQ host
    void hostIRQ();

    // Request and take bus
    BR_RETURN_TYPE controlReqAndTake();
    void controlRelease(bool resetTargetOnRelease);

    // Read and write bytes
    void byteWrite(uint32_t byte, int iorq);
    uint8_t byteRead(int iorq);

    // Read and write blocks
    BR_RETURN_TYPE blockWrite(uint32_t addr, const uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq);
    BR_RETURN_TYPE blockRead(uint32_t addr, uint8_t* pData, uint32_t len, int busRqAndRelease, int iorq);
    
    // Enable WAIT
    void waitEnableMemAndIO(bool enWaitOnIORQ, bool enWaitOnMREQ);
    void waitStateISR(void* pData);

    // Clear IO
    void clearIO();

    // Service
    void service();

    // Set and remove callbacks on bus access
    void accessCallbackSet(BusAccessCBFnType* pBusAccessCallback);
    void accessCallbackRemove();

    // Single step
    void pauseGetCurrent(uint32_t* pAddr, uint32_t* pData, uint32_t* pFlags);
    bool pauseIsPaused();
    bool pauseRelease();

private:
    // Timouts
    static const int MAX_WAIT_FOR_ACK_US = 100;

private:
    // Helpers
    void setPinOut(CGPIOPin& gpioPin, int pinNumber, bool val);
    void setPinIn(CGPIOPin& gpioPin, int pinNumber);
    void muxSet(int muxVal);
    void muxClear();
    void pinRawMode(int pinNumber, bool inputMode, bool val);
    void pinRawWrite(int pinNumber, bool val);
    bool pinRawRead(int pinNumber);

    // Set address
    void addrLowSet(uint32_t lowAddrByte);
    void addrLowInc();
    void addrHighSet(uint32_t highAddrByte);
    void addrSet(unsigned int addr);

    // Control the PIB (bus used to transfer data to/from Pi)
    void pibSetOut();
    void pibSetIn();
    void pibSetValue(uint8_t val);
    uint8_t pibGetValue();

    // Wait interrupts
    void waitIntClear();
    void waitIntDisable();
    void waitIntEnable();

    // Bus request/ack
    void controlRequestBus();
    int controlBusAcknowledged();
    void controlTake();

private:
    // Callback on bus access
    static BusAccessCBFnType* _pBusAccessCallback;

    // Current wait state mask for restoring wait enablement after change
    uint32_t _waitStateEnMask;

    // Wait interrupt enablement cache (so it can be restored after disable)
    bool _waitIntEnabled;

    // Currently paused - i.e. wait is active
    volatile bool __br_pause_is_paused = false;

    // Bus values while single stepping
    uint32_t _pauseCurAddr;
    uint32_t _pauseCurData;
    uint32_t _pauseCurControlBus;

    // Mux
    static const int NUM_MUX_PINS = 3;
    CGPIOPin _pinMux[NUM_MUX_PINS];

    // Bus req / ack
    CGPIOPin _pinBusReq;
    CGPIOPin _pinBusAck;

    // PIB (bus used to access data and addr busses)
    static const int NUM_PIB_PINS = 8;
    CGPIOPin _pinPIB[NUM_PIB_PINS];

    // Control bus
    CGPIOPin _pinMREQ;
    CGPIOPin _pinIORQ;
    CGPIOPin _pinRD;
    CGPIOPin _pinWR;

    // Wait states enable pins
    CGPIOPin _pinWaitMREQ;
    CGPIOPin _pinWaitIORQ;

    // Address and data handling pins
    CGPIOPin _pinAddrPush;
    CGPIOPin _pinAddrHighClock;
    CGPIOPin _pinAddrLowClock;
    CGPIOPin _pinDataDirIn;

    // Debug pin
    CGPIOPin _pinDebugCEO;

};
