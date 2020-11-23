// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>

// Module name
static const char MODULE_PREFIX[] = "BusAccWait";

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
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitonMem %d", isOn);
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
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitOnIO");
    waitEnablementUpdate();
}

// Suspend wait system
// Used by self-test to suspend normal wait operation
void BusAccess::waitSystemSuspend(bool suspend)
{
    waitEnablementUpdate(suspend);
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
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitRelease");
    waitResetFlipFlops();
    // Handle release after a read
    waitHandleReadRelease();
}

bool BusAccess::waitIsHeld()
{
    return _waitHold;
}

void BusAccess::waitHold(int busSocket, bool hold)
{
    // LogWrite(MODULE_PREFIX, LOG_DEBUG, "waitHold %d", hold);
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
// Wait helper function
// Update wait enablement based on requested WAIT from bus-sockets
// This may be done immediately after a change of machine or after self-testing where WAIT was disabled
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitEnablementUpdate(bool forceSuspend)
{
    // Iterate bus sockets to see if any enable Mem/IO wait states
    bool ioWait = false;
    bool memWait = false;
    if (!forceSuspend)
    {
        for (int i = 0; i < _busSocketCount; i++)
        {
            if (_busSockets[i].enabled)
            {
                memWait = memWait || _busSockets[i].waitOnMemory;
                ioWait = ioWait || _busSockets[i].waitOnIO;
            }
        }
    }

    // Store flags
    _waitOnMemory = memWait;
    _waitOnIO = ioWait;

    // LogWrite("BusAccess", LOG_DEBUG, "WAIT ENABLEMENT mreq %d iorq %d", memWait, ioWait);

    // Set PWM generator idle value to enable/disable wait states
    // This is done using the idle states of the PWM
    uint32_t pwmCtrl = read32(ARM_PWM_CTL);
    pwmCtrl &= ~(ARM_PWM_CTL_SBIT1 | ARM_PWM_CTL_SBIT2);
    if (_waitOnIO)
        pwmCtrl |= ARM_PWM_CTL_SBIT1;
    if (_waitOnMemory)
        pwmCtrl |= ARM_PWM_CTL_SBIT2;
    write32(ARM_PWM_CTL, pwmCtrl);

    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "WAIT UPDATE mem %d io %d", _waitOnMemory, _waitOnIO);

#ifdef ISR_TEST
    // Setup edge triggering on falling edge of IORQ
    // Clear any current detected edges
    write32(ARM_GPIO_GPEDS0, (1 << BR_IORQ_BAR) | (1 << BR_MREQ_BAR));  
    // Set falling edge detect
    if (_waitOnIO)
    {
        write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) | BR_IORQ_BAR_MASK);
        lowlev_enable_fiq();
    }
    else
    {
        write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) & (~BR_IORQ_BAR_MASK));
        lowlev_disable_fiq();
    }
    // if (_waitOnMemory)
    //     write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) | BR_WAIT_BAR_MASK);
    // else
    //     write32(ARM_GPIO_GPFEN0, read32(ARM_GPIO_GPFEN0) & (~BR_WAIT_BAR_MASK);
#endif
}

void BusAccess::waitGenerationDisable()
{
    // Debug
    // LogWrite("BusAccess", LOG_DEBUG, "WAIT DISABLE");
    // Set PWM idle state to disable waits
    uint32_t pwmCtrl = read32(ARM_PWM_CTL);
    pwmCtrl &= ~(ARM_PWM_CTL_SBIT1 | ARM_PWM_CTL_SBIT2);
    write32(ARM_PWM_CTL, pwmCtrl);
}

void BusAccess::waitResetFlipFlops(bool forceClear)
{
    // LogWrite("BusAccess", LOG_DEBUG, "WAIT waitResetFlipFlops");

    // Since the FIFO is shared the data output to MREQ/IORQ enable pins will be interleaved so we need to write data for both
    if ((read32(ARM_PWM_STA) & 1) == 0)
    {
        // Write to FIFO
        uint32_t busVals = read32(ARM_GPIO_GPLEV0);
        bool ioWaitClear = (forceClear || ((busVals & BR_IORQ_BAR_MASK) == 0)) && _waitOnIO;
        write32(ARM_PWM_FIF1, ioWaitClear ? 0x00ffffff : 0);  // IORQ sequence
        bool memWaitClear = (forceClear || ((busVals & BR_MREQ_BAR_MASK) == 0)) && _waitOnMemory;
        write32(ARM_PWM_FIF1, memWaitClear ? 0x00ffffff : 0);  // MREQ sequence
    }

    // Clear flag
    _waitIsActive = false;
}

void BusAccess::waitClearDetected()
{
    // TODO maybe no longer needed as service or timer used instead of edge detection???
    // Clear currently detected edge
    // write32(ARM_GPIO_GPEDS0, BR_ANY_EDGE_MASK);
}

void BusAccess::waitSuspendBusDetailOneCycle()
{
    if (_hwVersionNumber == 17)
        _waitSuspendBusDetailOneCycle = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait system initialzation code
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::waitSystemInit()
{
    // Use PWM to generate pulses as the behaviour of the processor is non-deterministic in terms
    // of time taken between consecutive instructions and when MREQ/IORQ enable is low wait
    // states are disabled and bus operations can be missed

    // Clock for PWM 
    uint32_t clockSource = ARM_CM_CTL_CLKSRC_PLLD;
    uint32_t freqReqd = 31250000;

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
    LogWrite("BusAccess", LOG_DEBUG, "PWM div %d, busyCount %d, lastBusy %08x afterKill %08x", divisor, busyCount, lastBusy, afterKill);
}
