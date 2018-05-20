
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
#define BUS_ACC_MUX_BUS_MASK (~((uint32_t)0xff << BUS_ACC_MUX_BUS_P0))

void muxBusOutputs()
{
	for (int i = 0; i < 8; i++)
	{
		pinMode(BUS_ACC_MUX_BUS_P0 + i, OUTPUT);
	}	
}

void muxBusInputs()
{
	for (int i = 0; i < 8; i++)
	{
		pinMode(BUS_ACC_MUX_BUS_P0 + i, INPUT);
	}	
}

void setMuxBus(uint8_t val)
{
// #ifdef USE_BITWISE_BUS_ACCESS	
	for (int i = 0; i < 8; i++)
	{
		digitalWrite(BUS_ACC_MUX_BUS_P0 + i, val & 0x01);
		val = val >> 1;
	}
// #else
// 	W32(GPLEV0, (R32(GPLEV0) & BUS_ACC_MUX_BUS_MASK) | (((uint32_t)val) << BUS_ACC_MUX_BUS_P0));
// #endif
}

uint8_t getMuxBus()
{
#ifdef USE_BITWISE_BUS_ACCESS
	uint8_t val = 0;
	for (int i = 0; i < 8; i++)
	{
		val = val | (digitalRead(BUS_ACC_MUX_BUS_P0 + i) & 0x01);
		val = val << 1;
	}	
#else
	uint32_t busVals = R32(GPLEV0);
	return (busVals >> BUS_ACC_MUX_BUS_P0) & 0xff;
#endif
}

void setAddrHigh(uint32_t val)
{
	setMuxBus(val);
	digitalWrite(BUS_ACC_H_ADDR_SET, 1);
	digitalWrite(BUS_ACC_H_ADDR_SET, 0);
}

void setAddrLow(uint32_t val)
{
	setMuxBus(val);
	digitalWrite(BUS_ACC_L_ADDR_SET, 1);
	digitalWrite(BUS_ACC_L_ADDR_SET, 0);
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
	pinMode(BUS_ACC_WR_BAR, INPUT);
	pinMode(BUS_ACC_RD_BAR, INPUT);
	pinMode(BUS_ACC_MREQ_BAR, INPUT);
	pinMode(BUS_ACC_IORQ_BAR, INPUT);
	digitalWrite(BUS_ACC_CTRL_IN, 1);
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

void busTakeControl()
{
	setPinOutAndValue(BUS_ACC_WR_BAR, 1);
	setPinOutAndValue(BUS_ACC_RD_BAR, 1);
	setPinOutAndValue(BUS_ACC_MREQ_BAR, 1);
	setPinOutAndValue(BUS_ACC_IORQ_BAR, 1);
	digitalWrite(BUS_ACC_CTRL_IN, 0);
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
	digitalWrite(BUS_ACC_DATA_DIRN_IN, 0);
	digitalWrite(BUS_ACC_DATA_BAR, 0);
	digitalWrite(BUS_ACC_MREQ_BAR, 0);
	digitalWrite(BUS_ACC_WR_BAR, 0);
	usleep(1);
	digitalWrite(BUS_ACC_WR_BAR, 1);
	digitalWrite(BUS_ACC_MREQ_BAR, 1);
	digitalWrite(BUS_ACC_DATA_BAR, 1);
	digitalWrite(BUS_ACC_DATA_DIRN_IN, 1);
	muxBusInputs();
}

uint8_t busReadData()
{
	digitalWrite(BUS_ACC_DATA_DIRN_IN, 1);
	digitalWrite(BUS_ACC_DATA_BAR, 0);
	digitalWrite(BUS_ACC_MREQ_BAR, 0);
	digitalWrite(BUS_ACC_RD_BAR, 0);
	usleep(1);
	uint8_t val = getMuxBus();
	digitalWrite(BUS_ACC_RD_BAR, 1);
	digitalWrite(BUS_ACC_MREQ_BAR, 1);
	digitalWrite(BUS_ACC_DATA_BAR, 1);
	return val;
}


