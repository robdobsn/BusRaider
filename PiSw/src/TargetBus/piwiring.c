// Bus Raider Wiring Library (like Arduino)
// Rob Dobson 2018

#include "piwiring.h"
#include "../System/bare_metal_pi_zero.h"
#include "../System/lowlev.h"
#include "../System/lowlib.h"

uint8_t convModeToVal(uint8_t mode)
{
    switch (mode) {
    case INPUT:
        return 0;
    case INPUT_PULLUP:
        return 0x20;
    case INPUT_PULLDOWN:
        return 0x10;
    case OUTPUT:
        return 1;
    case PINMODE_ALT0:
        return 4;
    case PINMODE_ALT1:
        return 5;
    case PINMODE_ALT2:
        return 6;
    case PINMODE_ALT3:
        return 7;
    case PINMODE_ALT4:
        return 3;
    case PINMODE_ALT5:
        return 2;
    }
    return 0;
}

void pinMode(uint8_t pin, uint8_t mode)
{
    uint32_t gpfSelReg = GPFSEL0 + (pin / 10) * 4;
    uint8_t bitPos = ((pin - ((pin / 10) * 10)) % 10) * 3;
    uint32_t regVal = R32(gpfSelReg);
    regVal &= ~(7 << bitPos);
    uint8_t modeVal = convModeToVal(mode);
    regVal |= (modeVal & 0x0f) << bitPos;
    W32(gpfSelReg, regVal);
    // Check for pull-up / pull-down
    if ((modeVal & 0xf0) != 0) {
        W32(GPPUD, modeVal >> 4);
        lowlev_cycleDelay(150);
        if (pin < 32)
            W32(GPPUDCLK0, 1 << pin);
        else
            W32(GPPUDCLK1, 1 << (pin - 32));
        lowlev_cycleDelay(150);
        W32(GPPUD, 0);
        if (pin < 32)
            W32(GPPUDCLK0, 0);
        else
            W32(GPPUDCLK1, 0);
    }
}

void digitalWrite(uint8_t pin, int val)
{
    if (val) {
        if (pin < 32)
            W32(GPSET0, 1 << pin);
        else
            W32(GPSET1, 1 << (pin - 32));
    } else {
        if (pin < 32)
            W32(GPCLR0, 1 << pin);
        else
            W32(GPCLR1, 1 << (pin - 32));
    }
}

int digitalRead(uint8_t pin)
{
    if (pin < 32)
        return ((R32(GPLEV0) >> pin) & 0x01) != 0;
    return ((R32(GPLEV1) >> (pin - 32)) & 0x01) != 0;
}