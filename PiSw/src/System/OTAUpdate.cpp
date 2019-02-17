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
    // Copy the blockCopyExecRelocatable() code to otaUpdateBuffer
    uint8_t* pCopyBlockNewLocation = __otaUpdateBuffer;
    memcpy((void*)pCopyBlockNewLocation, (void*)lowlev_blockCopyExecRelocatable, lowlev_blockCopyExecRelocatableLen);

    // Copy the received data to otaUpdateBuffer
    uint8_t* pRxDataNewLocation = __otaUpdateBuffer + lowlev_blockCopyExecRelocatableLen;
    memcpy((void*)pRxDataNewLocation, (void*)pData, dataLen);

    // Access the copyblock function in it's new location
    lowlev_blockCopyExecRelocatableFnT* pCopyBlockFn = (lowlev_blockCopyExecRelocatableFnT*) pCopyBlockNewLocation;
    LogWrite(FromOTAUpdate, LOG_DEBUG, "Address of copyBlockFn %08x, len %d", pCopyBlockNewLocation, lowlev_blockCopyExecRelocatableLen);

    // // Debug
    // int diffs = 0;
    // static const int MAX_DIFFS = 10;
    // uint32_t diffAddr[MAX_DIFFS];
    // uint8_t d1[MAX_DIFFS];
    // uint8_t d2[MAX_DIFFS];
    // uint8_t* p1 = (uint8_t*)0x8000;
    // uint8_t* p2 = pRxDataNewLocation;
    // for (int i = 0; i < dataLen; i++)
    // {
    //     if (*p1++ != *p2++)
    //     {
    //         if (diffs >= MAX_DIFFS)
    //         {
    //             diffs++;
    //             continue;
    //         }
    //         diffAddr[diffs] = (uint32_t)p1;
    //         d1[diffs] = *p1;
    //         d2[diffs] = *p2;
    //         diffs++;
    //     }
    // }
    // LogWrite(FromOTAUpdate, LOG_DEBUG, "DIFFS %d DataLoc %04x", diffs, pRxDataNewLocation);
    // for (int i = 0; i < diffs && i < MAX_DIFFS; i++)
    //     LogWrite(FromOTAUpdate, LOG_DEBUG, "%04x %02x %02x", diffAddr[i], d1[i], d2[i]);
    
    // // bool cpyit = micros() & 1;
    // // LogWrite(FromOTAUpdate, LOG_DEBUG, "cpyit %d", cpyit);
    // microsDelay(1000000);

    // if (!cpyit)
    //     lowlev_goto(0x8000);

    // Disable interrupts
    lowlev_disable_irq();
    lowlev_disable_fiq();

    // Data memory and instruction barriers (to force all memory operations to complete
    lowlev_dmb();
    lowlev_dsb();
    lowlev_flushcache();

    // Call the copyBlock function in its new location using it to move the program
    // to 0x8000 the base address for Pi programs
    (*pCopyBlockFn) ((uint8_t*)0x8000, pRxDataNewLocation, dataLen, (uint8_t*)0x8000);
}
