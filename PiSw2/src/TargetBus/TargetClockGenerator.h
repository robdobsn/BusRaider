
#pragma once

#include "logging.h"
#include "BusAccess.h"
#include "PiWiring.h"
#include "lowlib.h"
#include <circle/bcm2835.h>
#include <circle/memio.h>

// Following documented here: https://www.scribd.com/doc/127599939/BCM2835-Audio-clocks
#define ARM_CM_PCMCTL		(ARM_CM_BASE + 0x98)
#define ARM_CM_PCMDIV		(ARM_CM_BASE + 0x9C)
#define ARM_CM_PWMCTL		(ARM_CM_BASE + 0xA0)
#define ARM_CM_PWMDIV		(ARM_CM_BASE + 0xA4)

#define ARM_CM_CTL_PLLD_FREQ 500000000
#define ARM_CM_CTL_OSCILLATOR_FREQ 19200000
#define ARM_CM_CTL_MIN_FREQ_FOR_PLLD 250000
#define ARM_CM_CTL_CLKSRC_OSCILLATOR     0x01
#define ARM_CM_CTL_CLKSRC_TEST0     0x02
#define ARM_CM_CTL_CLKSRC_TEST1     0x03
#define ARM_CM_CTL_CLKSRC_PLLA     0x04
#define ARM_CM_CTL_CLKSRC_PLLC     0x05
#define ARM_CM_CTL_CLKSRC_PLLD     0x06
#define ARM_CM_CTL_CLKSRC_HDMIAUX     0x07
#define ARM_CM_CTL_CLK_SET_MASK 0x0f0f
#define ARM_CM_CTL_KILL     (1 << 5)
#define ARM_CM_CTL_ENAB     (1 << 4)
#define ARM_CM_CTL_BUSY     (1 << 7)

class TargetClockGenerator
{
  public:
    uint32_t _freqReqd;
    bool _enabled;
    int _outputPin;
    int _altMode;

    TargetClockGenerator()
    {
        _freqReqd = 1000000;
        _enabled = false;
        _outputPin = -1;
        _altMode = INPUT;
    }   

    bool setOutputPin(int pin = -1)
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

    uint32_t getMinFreqHz()
    {
        return 1;
    }

    uint32_t getMaxFreqHz()
    {
        return 25000000;
    }

    void setFrequency(uint32_t newFreq)
    {
        bool isEnabled = _enabled;
        LogWrite("ClockGen", LOG_DEBUG, "setFrequency %d curEnabled %d",
                newFreq, isEnabled);

        if (_enabled)
            enable(false);
        _freqReqd = newFreq;
        if (isEnabled)
            enable(true);
    }

    void enable(bool en)
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
            // LogWrite("ClockGen", LOG_VERBOSE, "Disabled");
            return;
        }

        // Validity
        if (_outputPin == -1 || _altMode == INPUT)
        {
            _enabled = false;
            LogWrite("ClockGen", LOG_ERROR, "Invalid settings pin %d mode %d",
                            _outputPin, _altMode);
            return;
        }

        // Wait a little if busy
        int busyCount = 0;
        static const int MAX_BUSY_WAIT_COUNT = 100;
        uint32_t lastBusy = 0;
        for (int i = 0; i < MAX_BUSY_WAIT_COUNT; i++)
        {
            if ((read32(ARM_CM_GP0CTL) & ARM_CM_CTL_BUSY) == 0)
                break;
            microsDelay(1);
            busyCount++;
            lastBusy = read32(ARM_CM_PWMCTL);
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
        uint32_t freqGenerated = sourceFreq / divisor;
        LogWrite("ClockGen", LOG_NOTICE, "Generated freq %d (req %d src %d div %d) pin %d mode %d busyCount %d lastBusy %08x",
                        freqGenerated, _freqReqd, sourceFreq, divisor, _outputPin, _altMode, busyCount, lastBusy);
    }

    uint32_t getFreqInHz()
    {
        return _freqReqd;
    }

};