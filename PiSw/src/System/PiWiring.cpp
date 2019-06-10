// Bus Raider Wiring Library (like Arduino)
// Rob Dobson 2018

#include "PiWiring.h"
#include "BCM2835.h"
#include "lowlev.h"
#include "lowlib.h"

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
    uint32_t gpfSelReg = ARM_GPIO_GPFSEL0 + (pin / 10) * 4;
    uint8_t bitPos = ((pin - ((pin / 10) * 10)) % 10) * 3;
    uint32_t regVal = RD32(gpfSelReg);
    regVal &= ~(7 << bitPos);
    uint8_t modeVal = convModeToVal(mode);
    regVal |= (modeVal & 0x0f) << bitPos;
    WR32(gpfSelReg, regVal);
    // Check for pull-up / pull-down
    if ((modeVal & 0xf0) != 0) {
        WR32(ARM_GPIO_GPPUD, modeVal >> 4);
        lowlev_cycleDelay(150);
        if (pin < 32)
            WR32(ARM_GPIO_GPPUDCLK0, 1 << pin);
        else
            WR32(ARM_GPIO_GPPUDCLK1, 1 << (pin - 32));
        lowlev_cycleDelay(150);
        WR32(ARM_GPIO_GPPUD, 0);
        if (pin < 32)
            WR32(ARM_GPIO_GPPUDCLK0, 0);
        else
            WR32(ARM_GPIO_GPPUDCLK1, 0);
    }
}

void digitalWrite(uint8_t pin, int val)
{
    if (val) {
        if (pin < 32)
            WR32(ARM_GPIO_GPSET0, 1 << pin);
        else
            WR32(ARM_GPIO_GPSET1, 1 << (pin - 32));
    } else {
        if (pin < 32)
            WR32(ARM_GPIO_GPCLR0, 1 << pin);
        else
            WR32(ARM_GPIO_GPCLR1, 1 << (pin - 32));
    }
}

int digitalRead(uint8_t pin)
{
    if (pin < 32)
        return ((RD32(ARM_GPIO_GPLEV0) >> pin) & 0x01) != 0;
    return ((RD32(ARM_GPIO_GPLEV1) >> (pin - 32)) & 0x01) != 0;
}