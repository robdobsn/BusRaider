
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void pinRawWrite(int pinNumber, bool val)
{
    if (val) {
        if (pinNumber < 32)
            write32(ARM_GPIO_GPSET0, 1 << pinNumber);
        else
            write32(ARM_GPIO_GPSET0+0x04, 1 << (pinNumber - 32));
    } else {
        if (pinNumber < 32)
            write32(ARM_GPIO_GPCLR0, 1 << pinNumber);
        else
            write32(ARM_GPIO_GPCLR0+0x04, 1 << (pinNumber - 32));
    }
}

void pinRawMode(int pinNumber, bool inputMode, bool val)
{
    if (!inputMode)
        pinRawWrite(pinNumber, val);
    uint32_t gpfSelReg = ARM_GPIO_GPFSEL0 + (pinNumber / 10) * 4;
    uint8_t bitPos = ((pinNumber - ((pinNumber / 10) * 10)) % 10) * 3;
    uint32_t regVal = read32(gpfSelReg);
    regVal &= ~(7 << bitPos);
    uint8_t modeVal = inputMode ? 0 : 1;
    regVal |= (modeVal & 0x0f) << bitPos;
    write32(gpfSelReg, regVal);
    if (!inputMode)
        pinRawWrite(pinNumber, val);
}

bool pinRawRead(int pinNumber)
{
    if (pinNumber < 32)
        return ((read32(ARM_GPIO_GPLEV0) >> pinNumber) & 0x01) != 0;
    return ((read32(ARM_GPIO_GPLEV0+0x04) >> (pinNumber - 32)) & 0x01) != 0;
}

uint32_t micros()
{
    static const uint32_t* pTimerLower32Bits = (uint32_t*)0x20003004;
    return *pTimerLower32Bits;
}

void CKernel::testTiming(int secs)
{
    pinRawMode(7, false, 0);
	pinRawMode(8, false, 0);
	volatile uint32_t lastMicros = micros();
    uint32_t entryMicros = lastMicros;
	int curVal = 0;
	volatile uint32_t lastWrite = lastMicros;
	while (1)
    {
        pinRawWrite(7, curVal);
        curVal = !curVal;
        volatile uint32_t newMicros = micros();
        if (newMicros > lastMicros)
        {
            if (newMicros - lastMicros > 5)
            {
				pinRawWrite(8,1);
				pinRawWrite(8,1);
				CTimer::SimpleusDelay(10);
				pinRawWrite(8,0);
				pinRawWrite(8,0);
				CTimer::SimpleusDelay(10);
            }
        }
		if (micros() > lastWrite + 1000000)
		{
			m_Screen.Write(".", 1);
			lastWrite = micros();
		}
        if (micros() > secs * 1000000 + entryMicros)
            break;
        lastMicros = micros();
	}
}

// void CKernel::testTiming(int secs)
// {
//     pinRawMode(7, false, 0);
// 	pinRawMode(8, false, 0);
// 	volatile uint32_t lastMicros = micros();
//     uint32_t entryMicros = lastMicros;
// 	int curVal = 0;
// 	volatile uint32_t lastWrite = lastMicros;
// 	while (1)
//     {
//         pinRawWrite(7, curVal);
//         curVal = !curVal;
//         volatile uint32_t newMicros = micros();
//         if (newMicros > lastMicros)
//         {
//             if (newMicros - lastMicros > 5)
//             {
// 				pinRawWrite(8,1);
// 				pinRawWrite(8,1);
// 				CTimer::SimpleusDelay(10);
// 				pinRawWrite(8,0);
// 				pinRawWrite(8,0);
// 				CTimer::SimpleusDelay(10);
//             }
//         }
// 		if (micros() > lastWrite + 1000000)
// 		{
// 			m_Screen.Write(".", 1);
// 			lastWrite = micros();
// 		}
//         if (micros() > secs * 1000000 + entryMicros)
//             break;
//         lastMicros = micros();
// 	}
// }