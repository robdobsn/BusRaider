// Bus Raider
// OTA Update
// Rob Dobson 2019

#include "OTAUpdate.h"
#include "lowlev.h"
#include "lowlib.h"
#include "logging.h"
#include "memorymap.h"
#include <string.h>

static const char FromOTAUpdate[] = "OTAUpdate";

bool OTAUpdate::performUpdate(const uint8_t* pData, int dataLen)
{
    // Delay to allow sending of serial data, etc
    microsDelay(1000000);

    // Calculations
    uint8_t* pCopyBlockNewLocation = (unsigned char*)OTA_UPDATE_START;
    uint8_t* pRxDataNewLocation = pCopyBlockNewLocation + lowlev_blockCopyExecRelocatableLen;
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

    // int diffs = 0;
    // uint8_t* pOrig = (uint8_t*)0x8000;
    // for (int i = 0; i < dataLen; i++)
    // {
    //     if (i >= 998 && i < 1004)
    //         LogWrite(FromOTAUpdate, LOG_DEBUG, "%d %02x %02x", i, pData[i], *(pOrig+i));

    //     if (pRxDataNewLocation[i] != *(pOrig+i))
    //         diffs++;
    // }
    // LogWrite(FromOTAUpdate, LOG_DEBUG, "Num diffs %d", diffs);
    // Should actually never return
    return true;
}
