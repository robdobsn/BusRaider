
// The following code from
// https://barrgroup.com/Embedded-Systems/How-To/Memory-Test-Suite-C

#include <stdint.h>
#include <stddef.h>

uint8_t memTestDataBus(BusControl& busControl, uint32_t address);

uint32_t memTestAddressBus(BusControl& busControl, uint32_t baseAddress, unsigned long nBytes);

uint32_t memTestDevice(BusControl& busControl, uint32_t baseAddress, unsigned long nBytes);

// int memTestAll(uint32_t baseAddress, uint32_t numBytes);

