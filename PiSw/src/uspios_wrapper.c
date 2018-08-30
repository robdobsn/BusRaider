#include "../uspi/include/uspios.h"
#include "ee_printf.h"
#include "irq.h"
#include "nmalloc.h"
#include "raspihwconfig.h"
#include "timer.h"
#include "uart.h"

void* malloc(unsigned nSize) // result must be 4-byte aligned
{
    return nmalloc_malloc(nSize);
}

void free(void* pBlock)
{
    nmalloc_free(&pBlock);
}

void MsDelay(unsigned nMilliSeconds)
{
    delayMicroseconds(nMilliSeconds * 1000);
}

void usDelay(unsigned nMicroSeconds)
{
    delayMicroseconds(nMicroSeconds);
}

unsigned StartKernelTimer(unsigned nHzDelay, // in HZ units (see "system configuration" above)
    TKernelTimerHandler* pHandler,
    void* pParam, void* pContext) // handed over to the timer handler
{
    nHzDelay = nHzDelay * HZ;
    return timer_attach_handler(nHzDelay, pHandler, pParam, pContext);
}

void CancelKernelTimer(__attribute__((unused)) unsigned hTimer)
{
    // NOT IMPLEMENTED
    //ee_printf("* CancelKernelTimer *\n");
}

void ConnectInterrupt(unsigned nIRQ, TInterruptHandler* pHandler, void* pParam)
{
    nIRQ = nIRQ;
    irq_set_usb_handler(pHandler, pParam);
}

int SetPowerStateOn(unsigned nDeviceId) // "set power state" to "on", wait until completed
{
    if (RHW_SUCCESS != rhw_set_device_power((RHW_DEVICE)nDeviceId, RHW_POWER_ON)) {
        return 0;
    }
    delayMicroseconds(500000); // Wait some more for wireless keyboards startup time
    return 1;
}

int GetMACAddress(unsigned char Buffer[6]) // "get board MAC address"
{
    //ee_printf("* GetMacAddress *\n");

    if (RHW_SUCCESS != rhw_get_mac_address(Buffer))
        return 0;

    return 1;
}

void uspi_assertion_failed(const char* pExpr, const char* pFile, unsigned nLine)
{
    ee_printf("ASSERTION FAILED: %s, in %s (Line %d)\n", pExpr, pFile, nLine);

    while (1)
        delayMicroseconds(1000000);
}

void DebugHexdump(const void* pBuffer, unsigned nBufLen, const char* pSource /* = 0 */)
{
    ee_printf("Memory dump of %s:\n", pSource);
    uart_dump_mem((unsigned char*)pBuffer, (unsigned char*)(pBuffer) + nBufLen);
}
