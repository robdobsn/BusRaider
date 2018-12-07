
#pragma once

#include "globaldefs.h"
#include "ee_printf.h"
#include "busraider.h"
#include "piwiring.h"

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

    void setFrequency(uint32_t newFreq)
    {
        bool isEnabled = _enabled;
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

        // Disable in any case
        static const uint32_t clockGenPLLD = 6; // pp 107
        W32(ARM_CM_GP0CTL, ARM_CM_PASSWD | clockGenPLLD);
        if (!en)
            return;

        // Validity
        if (_outputPin == -1 || _altMode == INPUT)
            return;

        // Wait a little if busy
        static const uint32_t clockGenBusyMask = 0x80;
        for (int i = 0; i < 1000; i++)
        {
            if ((R32(ARM_CM_GP0CTL) & clockGenBusyMask) == 0)
                break;
        }

        // Set the output pin
        pinMode(_outputPin, _altMode);

        // Set the divisor
        uint32_t divisor = 500000000 / _freqReqd;
        if (divisor > 4095)
            divisor = 4095;
        W32(ARM_CM_GP0DIV, ARM_CM_PASSWD | divisor << 12);

        // Enable (or disable) as required
        static const uint32_t clockGenEnabledMask = 0x10;  // pp107
        uint32_t enMask = en ? clockGenEnabledMask : 0;
        W32(ARM_CM_GP0CTL, ARM_CM_PASSWD | enMask | clockGenPLLD);

        // Debug
        uint32_t freqGenerated = 500000000 / divisor;
        ee_printf("ClockGen: Enabled freqGen %d (req %d ... div= %d), pin %d, mode %d\n",
                        freqGenerated, _freqReqd, divisor, _outputPin, _altMode);
    }

};