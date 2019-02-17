// Bus Raider
// OTA Update
// Rob Dobson 2019

#include "OTAUpdate.h"
#include "lowlev.h"
#include "lowlib.h"
#include "ee_printf.h"
#include <string.h>

static const char FromOTAUpdate[] = "OTAUpdate";

// OTA update space
extern uint8_t* __otaUpdateBuffer;

void OTAUpdate::performUpdate(uint8_t* pData, int dataLen)
{
    // Calculations
    uint8_t* pCopyBlockNewLocation = __otaUpdateBuffer;
    uint8_t* pRxDataNewLocation = __otaUpdateBuffer + lowlev_blockCopyExecRelocatableLen;
    LogWrite(FromOTAUpdate, LOG_DEBUG, "Address of copyBlockFn %08x, len %d", pCopyBlockNewLocation, lowlev_blockCopyExecRelocatableLen);

    // Disable ints
    lowlev_disable_irq();
    lowlev_disable_fiq();
    disable_mmu_and_cache();

    // Copy the blockCopyExecRelocatable() code to otaUpdateBuffer
    memcpy((void*)pCopyBlockNewLocation, (void*)lowlev_blockCopyExecRelocatable, lowlev_blockCopyExecRelocatableLen);

    // Copy the received data to otaUpdateBuffer
    memcpy((void*)pRxDataNewLocation, (void*)pData, dataLen);

    // Access the copyblock function in it's new location
    lowlev_blockCopyExecRelocatableFnT* pCopyBlockFn = (lowlev_blockCopyExecRelocatableFnT*) pCopyBlockNewLocation;

    // Call the copyBlock function in its new location using it to move the program
    // to 0x8000 the base address for Pi programs
    (*pCopyBlockFn) ((uint8_t*)0x8000, pRxDataNewLocation, dataLen, (uint8_t*)0x8000);
}
