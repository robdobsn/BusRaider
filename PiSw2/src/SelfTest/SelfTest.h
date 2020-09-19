// Bus Raider
// Rob Dobson 2020

class Display;
class BusAccess;
class BusRaiderApp;

enum SelfTestRetType {
    SELF_TEST_RET_OK,
    SELF_TEST_RET_QUIT,
    SELF_TEST_RET_TIMEOUT
};

void selfTestMemory(BusRaiderApp* pBusRaiderApp, Display& display, BusAccess& busAccess);

// TODO 2020
// void testSelf_busrq();
// void testSelf_readSetBus(bool readMode);
// void testSelf_detailedBus();
// void testSelf_memory();
// void testSelf_busBits();
// bool testSelf_detailedBus_addr();
// SelfTestRetType testSelf_commonLoop();

