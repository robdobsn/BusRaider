// Bus Raider
// Rob Dobson 2019-2020

#include "BusClockGenerator.h"
#include "BusClockDefs.h"
#include "BusAccessDefs.h"
#include "PiWiring.h"
#include "lowlib.h"
#include "logging.h"
#include "DebugHelper.h"

// Module name
static const char MODULE_PREFIX[] = "TargClock";

// Debug
// #define DEBUG_TARGET_CLOCK_GEN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Target Clock
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BusClockGenerator::BusClockGenerator()
{
    _freqReqd = 1000000;
    _enabled = false;
    _outputPin = -1;
    _altMode = INPUT;
}

bool BusClockGenerator::setup(int pin)
{
    // Check for default
    if (pin == -1)
        pin = BR_CLOCK_PIN; 
    // Check pin valid
    int altMode = INPUT;
    if (pin == 4) altMode = PINMODE_ALT0;
    else if (pin == 20) altMode = PINMODE_ALT5;
    else if (pin == 32) altMode = PINMODE_ALT0;
    else if (pin == 34) altMode = PINMODE_ALT0;
    if (altMode == INPUT)
        return false;
    _altMode = altMode;
    bool isEnabled = _enabled;
    if (_enabled)
        enable(false);
    _outputPin = pin;
    if (isEnabled)
        enable(true);
    return true;
}

void BusClockGenerator::setFreqHz(uint32_t newFreq)
{
    bool isEnabled = _enabled;
#ifdef DEBUG_TARGET_CLOCK_GEN
    LogWrite(MODULE_PREFIX, LOG_DEBUG, "setFreqHz %d curEnabled %d",
            newFreq, isEnabled);
#endif

    if (_enabled)
        enable(false);
    _freqReqd = newFreq;
    if (isEnabled)
        enable(true);
}

void BusClockGenerator::enable(bool en)
{
    // Check no change  
    if (en == _enabled)
        return;

    // Using information from:
    // https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2835/BCM2835-ARM-Peripherals.pdf
    // Page 107

    // Disable initially without changing clock source or mash
    uint32_t curVal = read32(ARM_CM_GP0CTL);
    write32(ARM_CM_GP0CTL, ARM_CM_PASSWD | (curVal & ARM_CM_CTL_CLK_SET_MASK));
    if (!en)
    {
        _enabled = false;
#ifdef DEBUG_TARGET_CLOCK_GEN
        LogWrite(MODULE_PREFIX, LOG_DEBUG, "Disabled");
#endif
        return;
    }

    // Validity
    if (_outputPin == -1 || _altMode == INPUT)
    {
        _enabled = false;
        LogWrite(MODULE_PREFIX, LOG_ERROR, "Invalid settings pin %d mode %d",
                        _outputPin, _altMode);
        return;
    }

    // Wait a little if busy
    int busyCount = 0;
    static const int MAX_BUSY_WAIT_COUNT = 100;
#ifdef DEBUG_TARGET_CLOCK_GEN
    uint32_t lastBusy = 0;
#endif
    for (int i = 0; i < MAX_BUSY_WAIT_COUNT; i++)
    {
        if ((read32(ARM_CM_GP0CTL) & ARM_CM_CTL_BUSY) == 0)
            break;
        microsDelay(1);
        busyCount++;
#ifdef DEBUG_TARGET_CLOCK_GEN
        lastBusy = read32(ARM_CM_PWMCTL);
#endif
    }

    // Set the output pin
    pinMode(_outputPin, _altMode);

    // Check which clock source we're using 
    bool bUsePLLD = (_freqReqd >= ARM_CM_CTL_MIN_FREQ_FOR_PLLD);
    uint32_t sourceFreq = (bUsePLLD ? ARM_CM_CTL_PLLD_FREQ : ARM_CM_CTL_OSCILLATOR_FREQ);
    uint32_t clkSourceMask = (bUsePLLD ? ARM_CM_CTL_CLKSRC_PLLD : ARM_CM_CTL_CLKSRC_OSCILLATOR);

    // Set the source but keep disabled
    write32(ARM_CM_GP0CTL, ARM_CM_PASSWD | clkSourceMask);
    microsDelay(1000);

    // Set the divisor
    uint32_t divisor = sourceFreq / _freqReqd;
    if (divisor > 4095)
        divisor = 4095;
    write32(ARM_CM_GP0DIV, ARM_CM_PASSWD | divisor << 12);
    microsDelay(1000);
    
    // Enable (or disable) as required
    uint32_t enMask = en ? ARM_CM_CTL_ENAB : 0;
    write32(ARM_CM_GP0CTL, ARM_CM_PASSWD | enMask | clkSourceMask);
    _enabled = true;

    // Debug
#ifdef DEBUG_TARGET_CLOCK_GEN
    uint32_t freqGenerated = sourceFreq / divisor;
    LogWrite(MODULE_PREFIX, LOG_NOTICE, "Generated freq %d (req %d src %d div %d) pin %d mode %d busyCount %d lastBusy %08x",
                    freqGenerated, _freqReqd, sourceFreq, divisor, _outputPin, _altMode, busyCount, lastBusy);
#endif
}