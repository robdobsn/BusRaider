// Bus Raider
// Rob Dobson 2018-2019

#include "BusAccess.h"
#include "PiWiring.h"
#include "lowlev.h"
#include "lowlib.h"
#include <circle/bcm2835.h>

// Module name
static const char MODULE_PREFIX[] = "BusAccRaw";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// External API control
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BusAccess::rawBusControlEnable(bool en)
{
    busAccessReinit();
    _busServiceEnabled = !en;
}

void BusAccess::rawBusControlClearWait()
{
    // LogWrite("BusAccess", LOG_DEBUG, "rawBusControlClearWait");
    waitResetFlipFlops();
    // Handle release after a read
    waitHandleReadRelease();
}

void BusAccess::rawBusControlWaitDisable()
{
    waitGenerationDisable();
}

void BusAccess::rawBusControlClockEnable(bool en)
{
    if(!en)
    {
        clockEnable(false);
        pinMode(BR_CLOCK_PIN, OUTPUT);
    }
    else
    {
        clockSetup();
        clockSetFreqHz(1000000);
        clockEnable(true);
    }
}

bool BusAccess::rawBusControlTakeBus()
{
    return (controlRequestAndTake() == BR_OK);
}

void BusAccess::rawBusControlReleaseBus()
{
    controlRelease();
}

void BusAccess::rawBusControlSetAddress(uint32_t addr)
{
    addrSet(addr);
}

void BusAccess::rawBusControlSetData(uint32_t data)
{
    // Set the data onto the PIB
    pibSetOut();
    pibSetValue(data);
    // Perform the write
    // Clear DIR_IN (so make direction out), enable data output onto data bus and MREQ_BAR active
    write32(ARM_GPIO_GPCLR0, BR_DATA_DIR_IN_MASK | BR_MUX_CTRL_BIT_MASK);
    write32(ARM_GPIO_GPSET0, BR_MUX_DATA_OE_BAR_LOW << BR_MUX_LOW_BIT_POS);
    if (_hwVersionNumber == 17)
    {
        lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);
    }
    else
    {
        write32(ARM_GPIO_GPCLR0, BR_MUX_EN_BAR_MASK);
        lowlev_cycleDelay(CYCLES_DELAY_FOR_OUT_FF_SET);
        write32(ARM_GPIO_GPSET0, BR_MUX_EN_BAR_MASK);
        write32(ARM_GPIO_GPCLR0, BR_MUX_CTRL_BIT_MASK);       
    }
}

uint32_t BusAccess::rawBusControlReadRaw()
{
    return read32(ARM_GPIO_GPLEV0);
}

uint32_t BusAccess::rawBusControlReadCtrl()
{
    return controlBusRead();
}

void BusAccess::rawBusControlReadAll(uint32_t& ctrl, uint32_t& addr, uint32_t& data)
{
    ctrl = controlBusRead();
    addrAndDataBusRead(addr, data);
}

void BusAccess::rawBusControlSetPin(uint32_t pinNumber, bool level)
{
    digitalWrite(pinNumber, level);
}

bool BusAccess::rawBusControlGetPin(uint32_t pinNumber)
{
    return digitalRead(pinNumber);
}

uint32_t BusAccess::rawBusControlReadPIB()
{
    pibSetIn();
    return (read32(ARM_GPIO_GPLEV0) >> BR_DATA_BUS) & 0xff;
}

void BusAccess::rawBusControlWritePIB(uint32_t val)
{
    // Set the data onto the PIB
    pibSetOut();
    pibSetValue(val);
}

void BusAccess::rawBusControlMuxSet(uint32_t val)
{
    muxSet(val);
}

void BusAccess::rawBusControlMuxClear()
{
    muxClear();
}

void BusAccess::rawBusControlTargetReset(uint32_t ms)
{
    rawBusControlMuxSet(BR_MUX_RESET_Z80_BAR_LOW);
    microsDelay(100000);
    rawBusControlMuxClear();
}