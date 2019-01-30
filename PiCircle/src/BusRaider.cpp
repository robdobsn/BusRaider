// Bus Raider
// Rob Dobson 2018-2019

#include "BusRaider.h"
#include <assert.h>
#include <string.h>
#include "System/loghandler.h"

// Logger string
static const char FromBusRaider[] = "BusRaider";

// Singleton BusRaider instance pointer
BusRaider *BusRaider::s_pThis = 0;

BusRaider::BusRaider (void)
{
	s_pThis = this;

    // Command handler callbacks
    _commandHandler.setCallbacks(static_putBytesToSerial, static_doChangeMachine);
}

BusRaider::~BusRaider (void)
{
	s_pThis = 0;
}

// Callback to send bytes to serial interface
void BusRaider::static_putBytesToSerial(const uint8_t* pBytes, int numBytes)
{
    if (s_pThis)
        s_pThis->putBytesToSerial(pBytes, numBytes);
}
void BusRaider::putBytesToSerial(const uint8_t* pBytes, int numBytes)
{
}

// Callback when target machine is changed
void BusRaider::static_doChangeMachine(const char* mcName)
{
    if (s_pThis)
        s_pThis->doChangeMachine(mcName);
}
void BusRaider::doChangeMachine(const char* mcName)
{
    LogWrite(FromBusRaider, LogNotice, "Machine change to %s", mcName);
}

void BusRaider::handleSerialRxBuffer(const uint8_t* pBytes, int numBytes)
{
    _commandHandler.handleBuffer(pBytes, numBytes);
}
