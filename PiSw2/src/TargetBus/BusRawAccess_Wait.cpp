// Bus Raider
// Rob Dobson 2018-2019

#include "BusRawAccess.h"
#include "TargetClockDefs.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>

// Module name
static const char MODULE_PREFIX[] = "BusRawWait";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait system initialzation code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::waitSystemInit()
{
    // Use PWM to generate pulses as the behaviour of the processor is non-deterministic in terms
    // of time taken between consecutive instructions and when MREQ/IORQ enable is low wait
    // states are disabled and bus operations can be missed

    _waitOnMem_Socket = false;
    _waitOnIO_Socket = false;
    _waitOnMem_Debugger = false;
    _waitOnIO_Debugger = false;
    _waitIsSuspended = false;

    // Clock for PWM 
    uint32_t clockSource = ARM_CM_CTL_CLKSRC_PLLD;
    uint32_t freqReqd = 62500000;

    // Disable the clock (without changing SRC and FLIP)
    write32(ARM_CM_PWMCTL, ARM_CM_PASSWD | clockSource);

    // Wait a little if clock is busy
    int busyCount = 0;
    static const int MAX_BUSY_WAIT_COUNT = 100000;
    uint32_t lastBusy = 0;
    for (int i = 0; i < MAX_BUSY_WAIT_COUNT; i++)
    {
        if ((read32(ARM_CM_PWMCTL) & ARM_CM_CTL_BUSY) == 0)
            break;
        microsDelay(1);
        busyCount++;
        lastBusy = read32(ARM_CM_PWMCTL);
    }
    uint32_t afterKill = 0;
    if (busyCount == MAX_BUSY_WAIT_COUNT)
    {
        write32(ARM_CM_PWMCTL, ARM_CM_PASSWD | ARM_CM_CTL_KILL | clockSource);
        microsDelay(1);
        afterKill = read32(ARM_CM_PWMCTL);
    }

    // Set output pins
    // MREQ will use PWM channel 2 and IORQ PWM channel 1
    pinMode(BR_MREQ_WAIT_EN, PINMODE_ALT0);
    pinMode(BR_IORQ_WAIT_EN, PINMODE_ALT0);

    // Clear status on PWM generator
    write32(ARM_PWM_STA, 0xffffffff);

    // Set the divisor for PWM clock
    uint32_t divisor = ARM_CM_CTL_PLLD_FREQ / freqReqd;
    if (divisor > 4095)
        divisor = 4095;
    write32(ARM_CM_PWMDIV, ARM_CM_PASSWD | divisor << 12);
    microsDelay(1);

    // Enable PWM clock
    write32(ARM_CM_PWMCTL, ARM_CM_PASSWD | ARM_CM_CTL_ENAB | clockSource);
    microsDelay(1);

    // Clear wait state enables initially - leaving LOW disables wait state generation
    // Enable PWM two channels (one for IORQ and one for MREQ)
    // FIFO is cleared so pins for IORQ and MREQ enable will be low
    write32(ARM_PWM_CTL, ARM_PWM_CTL_CLRF1 | ARM_PWM_CTL_USEF2 | ARM_PWM_CTL_MODE2 | ARM_PWM_CTL_PWEN2 |
                                          ARM_PWM_CTL_USEF1 | ARM_PWM_CTL_MODE1 | ARM_PWM_CTL_PWEN1);
    microsDelay(1);

    // Debug
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "PWM div %d, busyCount %d, lastBusy %08x afterKill %08x", divisor, busyCount, lastBusy, afterKill);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configure WAIT processing for socket system
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE BusRawAccess::waitConfigSocket(bool waitOnMemory, bool waitOnIO)
{
    // Save the settings
    _waitOnMem_Socket = waitOnMemory;
    _waitOnIO_Socket = waitOnIO;
    waitRawSet();
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Configure WAIT processing for debugger
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BR_RETURN_TYPE BusRawAccess::waitConfigDebugger(bool waitOnMemory, bool waitOnIO)
{
    // Save the settings
    _waitOnMem_Debugger = waitOnMemory;
    _waitOnIO_Debugger = waitOnIO;
    waitRawSet();
    return BR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait clear detected
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::waitClearDetected()
{
    // TODO not currently needed as service or timer used instead of edge detection
    // Clear currently detected edge
    // write32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Suspend/restore wait
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::waitSuspend(bool suspend)
{
    _waitIsSuspended = suspend;
    waitRawSet();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait raw setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusRawAccess::waitRawSet()
{
    // Decide what to wait for
    bool waitOnMem = (_waitOnMem_Socket || _waitOnMem_Debugger) && !_waitIsSuspended;
    bool waitOnIO = (_waitOnIO_Socket || _waitOnIO_Debugger) && !_waitIsSuspended;

    // Set PWM generator idle value to enable/disable wait states
    // This is done using the idle states of the PWM
    uint32_t pwmCtrl = read32(ARM_PWM_CTL);
    pwmCtrl &= ~(ARM_PWM_CTL_SBIT1 | ARM_PWM_CTL_SBIT2);
    if (waitOnIO)
        pwmCtrl |= ARM_PWM_CTL_SBIT1;
    if (waitOnMem)
        pwmCtrl |= ARM_PWM_CTL_SBIT2;
    write32(ARM_PWM_CTL, pwmCtrl);
}
