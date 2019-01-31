// Bus Raider
// OTA Update
// Rob Dobson 2019

#include "OTAUpdate.h"
#include "lowlev.h"
#include "LogHandler.h"
#include <string.h>
#include <circle/debug.h>
#include <circle/timer.h>
#include <circle/synchronize.h>

static const char FromOTAUpdate[] = "OTAUpdate";

void OTAUpdate::performUpdate(uint8_t* pData, int dataLen)
{
    // Size to allocate
    int stackSize = 80;
    int sizeToAlloc = dataLen + lowlevBlockCopyExecRelocatableLen + stackSize;
    int stackOffset = dataLen + lowlevBlockCopyExecRelocatableLen + stackSize;
    // Allocate space on heap for "copyblock" code, new firmware and stack
    uint8_t* pOTABufferBegin = new uint8_t[sizeToAlloc];
    if (!pOTABufferBegin)
    {
        LogWrite(FromOTAUpdate, LOG_DEBUG, "Unable to alloc OTA buffer %d", dataLen + lowlevBlockCopyExecRelocatableLen);
        return;
    }

    // Copy the blockCopyExecRelocatable() code to otaUpdateBuffer
    uint8_t* pCopyBlockNewLocation = pOTABufferBegin;
    memcpy((void*)pCopyBlockNewLocation, (void*)lowlevBlockCopyExecRelocatable, lowlevBlockCopyExecRelocatableLen);

    // Copy the received data to otaUpdateBuffer
    uint8_t* pRxDataNewLocation = pOTABufferBegin + lowlevBlockCopyExecRelocatableLen;
    memcpy((void*)pRxDataNewLocation, (void*)pData, dataLen);

    LogWrite(FromOTAUpdate, LOG_DEBUG, "Address of otaBuffer %08x, len %d", 
                pOTABufferBegin, dataLen + lowlevBlockCopyExecRelocatableLen);
 
    // // Call the copyblock function in it's new location
    // lowlevBlockCopyExecRelocatableFnT* pCopyBlockFn = (lowlevBlockCopyExecRelocatableFnT*) pCopyBlockNewLocation;
    // LogWrite(FromOTAUpdate, LOG_DEBUG, "Address of copyBlockFn %08x, len %d", pCopyBlockNewLocation, lowlevBlockCopyExecRelocatableLen);

    LogWrite(FromOTAUpdate, LOG_DEBUG, "RxData");
    debug_hexdump ((void *) pData, 128, FromOTAUpdate);

    LogWrite(FromOTAUpdate, LOG_DEBUG, "0x8000");
    debug_hexdump ((void *) 0x8000, 128, FromOTAUpdate);

    LogWrite(FromOTAUpdate, LOG_DEBUG, "OTABuffer");
    debug_hexdump ((void *) pOTABufferBegin, 128, FromOTAUpdate);

    // uint8_t* p1 = pCopyBlockNewLocation + lowlevBlockCopyExecRelocatableLen;
    // uint8_t* p2 = (uint8_t*) 0x8000;
    int diffs = 0;
    // for (int i = 0; i < dataLen; i++)
    // {
    //     if (*p1 != *p2)
    //     {
    //         diffs++;
    //         LogWrite(FromOTAUpdate, LOG_DEBUG, "Diff at %08x %02x != %02x", p2, *p1, *p2);
    //     }
    //     p1++;
    //     p2++;
    // }
    
    // LogWrite(FromOTAUpdate, LOG_DEBUG, "NUM DIFFS %d", diffs);

    LogWrite(FromOTAUpdate, LOG_DEBUG, "Starting new firmware ...", diffs);
    CTimer::SimpleMsDelay(500);

    // // Disable interrupts
    // lowlevDisableFIQ();
    // lowlevDisableIRQ();

    // Barrier
    SyncDataAndInstructionCache();

    // Call the copyBlock function in its new location using it to move the program
    // to 0x8000 the base address for Pi programs
    // (*pCopyBlockFn) ((uint8_t*)0x8000, pRxDataNewLocation, dataLen, (uint8_t*)0x8000, pOTABufferBegin+stackOffset);
    lowlevBlockCopyExecRelocatable((uint8_t*)0x8000, pRxDataNewLocation, dataLen, (uint8_t*)0x8000, pOTABufferBegin+stackOffset);

    LogWrite(FromOTAUpdate, LOG_DEBUG, "Back again");
}
