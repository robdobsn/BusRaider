// Bus Raider
// Rob Dobson 2018-2019

#include "CommandHandler/CommandHandler.h"

class BusRaider
{
public:
	BusRaider ();
	~BusRaider ();

    static void static_doChangeMachine(const char* mcName);
    void doChangeMachine(const char* mcName);
    static void static_putBytesToSerial(const uint8_t* pBytes, int numBytes);
    void putBytesToSerial(const uint8_t* pBytes, int numBytes);

    void handleSerialRxBuffer(const uint8_t* pBytes, int numBytes);

private:
	static BusRaider *s_pThis;

    CommandHandler _commandHandler;
};
