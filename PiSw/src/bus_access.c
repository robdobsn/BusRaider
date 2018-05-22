
#include "piwiring.h"
#include "timer.h"
#include "ee_printf.h"

//#define USE_BITWISE_BUS_ACCESS 1

#define BUS_ACC_MUX_BUS_P0 20
#define BUS_ACC_WR_BAR 17
#define BUS_ACC_RD_BAR 18
#define BUS_ACC_MREQ_BAR 0
#define BUS_ACC_IORQ_BAR 1
#define BUS_ACC_H_ADDR_SET 9
#define BUS_ACC_L_ADDR_SET 8
#define BUS_ACC_RESET 10
#define BUS_ACC_PUSH_ADDR_BAR 7
#define BUS_ACC_WAIT_OR_NMI 4
#define BUS_ACC_CTRL_IN 5
#define BUS_ACC_DATA_BAR 6
#define BUS_ACC_DATA_DIRN_IN 12
#define BUS_ACC_L_ADDR_BAR 13
#define BUS_ACC_H_ADDR_BAR 16
#define BUS_ACC_BUSRQ 19
#define BUS_ACC_BUSACK_BAR 2
#define BUS_ACC_MUX_BUS_MASK (~((uint32_t)0xff << BUS_ACC_MUX_BUS_P0))
#define BUS_ACC_MUX_BUS_GPF_REG GPFSEL2
#define BUS_ACC_MUX_BUS_GPF_MASK 0xff000000
#define BUS_ACC_MUX_BUS_GPF_INPUT 0x00000000
#define BUS_ACC_MUX_BUS_GPF_OUTPUT 0x00249249

void muxBusOutputs()
{
#ifdef USE_BITWISE_BUS_ACCESS
	for (int i = 0; i < 8; i++)
	{
		pinMode(BUS_ACC_MUX_BUS_P0 + i, OUTPUT);
	}
#else
	W32(BUS_ACC_MUX_BUS_GPF_REG, (R32(BUS_ACC_MUX_BUS_GPF_REG) & BUS_ACC_MUX_BUS_GPF_MASK) | BUS_ACC_MUX_BUS_GPF_OUTPUT);
#endif
}

void muxBusInputs()
{
#ifdef USE_BITWISE_BUS_ACCESS
	for (int i = 0; i < 8; i++)
	{
		pinMode(BUS_ACC_MUX_BUS_P0 + i, INPUT);
	}	
#else
	W32(BUS_ACC_MUX_BUS_GPF_REG, (R32(BUS_ACC_MUX_BUS_GPF_REG) & BUS_ACC_MUX_BUS_GPF_MASK) | BUS_ACC_MUX_BUS_GPF_INPUT);
#endif
}

void setMuxBus(uint8_t val)
{
#ifdef USE_BITWISE_BUS_ACCESS	
	uint32_t va0 = R32(GPLEV0);
	uint32_t va1 = (R32(GPLEV0) & BUS_ACC_MUX_BUS_MASK) | (((uint32_t)val) << BUS_ACC_MUX_BUS_P0);

	for (int i = 0; i < 8; i++)
	{
		digitalWrite(BUS_ACC_MUX_BUS_P0 + i, val & 0x01);
		val = val >> 1;
	}
#else
	uint32_t setBits = ((uint32_t)val) << BUS_ACC_MUX_BUS_P0;
	uint32_t clrBits = (~(((uint32_t)val) << BUS_ACC_MUX_BUS_P0)) & (~BUS_ACC_MUX_BUS_MASK);
	W32(GPSET0, setBits);
	W32(GPCLR0, clrBits);
#endif
}

uint8_t getMuxBus()
{
#ifdef USE_BITWISE_BUS_ACCESS
	uint8_t val = 0;
	for (int i = 0; i < 8; i++)
	{
		int bitVal = digitalRead(BUS_ACC_MUX_BUS_P0 + i);
		// ee_printf("%d ", bitVal);
		val = val >> 1;
		val = val | ((bitVal != 0) ? 0x80 : 0);
	}	
	// ee_printf("\n");
	return val;
#else
	uint32_t busVals = R32(GPLEV0);
	return (busVals >> BUS_ACC_MUX_BUS_P0) & 0xff;
#endif
}

void setAddrHigh(uint32_t val)
{
	setMuxBus(val);
#ifdef USE_BITWISE_BUS_ACCESS	
	digitalWrite(BUS_ACC_H_ADDR_SET, 1);
	digitalWrite(BUS_ACC_H_ADDR_SET, 0);
#else
	W32(GPSET0, 1 << BUS_ACC_H_ADDR_SET);
	W32(GPCLR0, 1 << BUS_ACC_H_ADDR_SET);
#endif
}

void setAddrLow(uint32_t val)
{
	setMuxBus(val);
#ifdef USE_BITWISE_BUS_ACCESS	
	digitalWrite(BUS_ACC_L_ADDR_SET, 1);
	digitalWrite(BUS_ACC_L_ADDR_SET, 0);
#else
	W32(GPSET0, 1 << BUS_ACC_L_ADDR_SET);
	W32(GPCLR0, 1 << BUS_ACC_L_ADDR_SET);
#endif
}

void setPinOutAndValue(int pin, int val)
{
	digitalWrite(pin, val);
	pinMode(pin, OUTPUT);
	digitalWrite(pin, val);
}

void busSetup()
{
	// Bus Request
	setPinOutAndValue(BUS_ACC_BUSRQ, 0);
	// RESET Z80
	setPinOutAndValue(BUS_ACC_RESET, 0);
	// NMI or WAIT
	setPinOutAndValue(BUS_ACC_WAIT_OR_NMI, 0);
	// Low address Output enable
	setPinOutAndValue(BUS_ACC_L_ADDR_BAR, 1);
	// High address Output enable
	setPinOutAndValue(BUS_ACC_H_ADDR_BAR, 1);
	// Low address latch
	setPinOutAndValue(BUS_ACC_L_ADDR_SET, 0);
	// High address latch
	setPinOutAndValue(BUS_ACC_H_ADDR_SET, 0);
	// Data bus direction 
	setPinOutAndValue(BUS_ACC_DATA_DIRN_IN, 1);
	// Read data Output enable
	setPinOutAndValue(BUS_ACC_DATA_BAR, 1);
	// Control bus direction 
	setPinOutAndValue(BUS_ACC_CTRL_IN, 1);
	// Address push
	setPinOutAndValue(BUS_ACC_PUSH_ADDR_BAR, 1);

	ee_printf("%08x,", BUS_ACC_MUX_BUS_MASK);
	uint8_t val = 0x72;
	ee_printf("%08x\n", (0xAAAAAAAA & BUS_ACC_MUX_BUS_MASK) | (((uint32_t)val) << BUS_ACC_MUX_BUS_P0));
}

void busReleaseControl()
{
	digitalWrite(BUS_ACC_BUSRQ, 0);
	pinMode(BUS_ACC_WR_BAR, INPUT_PULLUP);
	pinMode(BUS_ACC_RD_BAR, INPUT_PULLUP);
	pinMode(BUS_ACC_MREQ_BAR, INPUT_PULLUP);
	pinMode(BUS_ACC_IORQ_BAR, INPUT_PULLUP);
	// digitalWrite(BUS_ACC_CTRL_IN, 1);
	digitalWrite(BUS_ACC_L_ADDR_BAR, 1);
	digitalWrite(BUS_ACC_H_ADDR_BAR, 1);
	digitalWrite(BUS_ACC_PUSH_ADDR_BAR, 1);
	digitalWrite(BUS_ACC_DATA_BAR, 1);
	muxBusInputs();
}

void busRequestControl()
{
	digitalWrite(BUS_ACC_BUSRQ, 1);
}

int busControlAcknowledged()
{
	return (R32(GPLEV0) & (1 << BUS_ACC_BUSACK_BAR)) == 0;
}

void busTakeControl()
{
	setPinOutAndValue(BUS_ACC_WR_BAR, 1);
	setPinOutAndValue(BUS_ACC_RD_BAR, 1);
	setPinOutAndValue(BUS_ACC_MREQ_BAR, 1);
	setPinOutAndValue(BUS_ACC_IORQ_BAR, 1);
	// digitalWrite(BUS_ACC_CTRL_IN, 0);
	digitalWrite(BUS_ACC_L_ADDR_BAR, 1);
	digitalWrite(BUS_ACC_H_ADDR_BAR, 1);
	digitalWrite(BUS_ACC_PUSH_ADDR_BAR, 0);
	digitalWrite(BUS_ACC_DATA_BAR, 1);
	muxBusInputs();
}

void busSetAddr(uint32_t addr)
{
	muxBusOutputs();
	setAddrHigh(addr >> 8);
	setAddrLow(addr);
	muxBusInputs();
}

void busWriteData(uint8_t data)
{
	muxBusOutputs();
	setMuxBus(data);
#ifdef USE_BITWISE_BUS_ACCESS
	digitalWrite(BUS_ACC_DATA_DIRN_IN, 0);
	digitalWrite(BUS_ACC_DATA_BAR, 0);
	digitalWrite(BUS_ACC_MREQ_BAR, 0);
	digitalWrite(BUS_ACC_WR_BAR, 0);
	usleep(1);
	digitalWrite(BUS_ACC_WR_BAR, 1);
	digitalWrite(BUS_ACC_MREQ_BAR, 1);
	digitalWrite(BUS_ACC_DATA_BAR, 1);
	digitalWrite(BUS_ACC_DATA_DIRN_IN, 1);
#else
	W32(GPCLR0, (1 << BUS_ACC_DATA_DIRN_IN) | (1 << BUS_ACC_DATA_BAR) | (1 << BUS_ACC_MREQ_BAR));
	W32(GPCLR0, (1 << BUS_ACC_WR_BAR));
	W32(GPSET0, (1 << BUS_ACC_DATA_DIRN_IN) | (1 << BUS_ACC_DATA_BAR) | (1 << BUS_ACC_MREQ_BAR) | (1 << BUS_ACC_WR_BAR));
#endif
	muxBusInputs();
}

uint8_t busReadData()
{
#ifdef USE_BITWISE_BUS_ACCESS
	digitalWrite(BUS_ACC_DATA_DIRN_IN, 1);
	digitalWrite(BUS_ACC_DATA_BAR, 0);
	digitalWrite(BUS_ACC_MREQ_BAR, 0);
	digitalWrite(BUS_ACC_RD_BAR, 0);
	usleep(1);
	digitalWrite(BUS_ACC_RD_BAR, 1);
	digitalWrite(BUS_ACC_MREQ_BAR, 1);
	digitalWrite(BUS_ACC_DATA_BAR, 1);
#else
	W32(GPCLR0, (1 << BUS_ACC_DATA_BAR) | (1 << BUS_ACC_MREQ_BAR) | (1 << BUS_ACC_RD_BAR));
	uint8_t val = getMuxBus();
	W32(GPSET0, (1 << BUS_ACC_DATA_BAR) | (1 << BUS_ACC_MREQ_BAR) | (1 << BUS_ACC_RD_BAR));
#endif
	return val;
}


