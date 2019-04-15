// Bus Raider
// Rob Dobson 2019

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include "../System/ee_sprintf.h"
#include "../System/logging.h"

outChFnType* __ee_pOutFunction = NULL;

void LogSetOutFn(outChFnType* pOutFn)
{
    __ee_pOutFunction = pOutFn;
}

#define DISP_WRITE_STRING(x) if(__ee_pOutFunction) (*__ee_pOutFunction)((const char*)x)
#define LOG_WRITE_STRING(x) if(__ee_pOutFunction) (*__ee_pOutFunction)((const char*)x)

#define USPI_LOG_SEVERITY LOG_VERBOSE

unsigned int __logSeverity = LOG_DEBUG;

void LogWrite(const char* pSource,
    unsigned severity,
    const char* fmt, ...)
{
    // Check for USPI log messages
    if (strstr(pSource, "uspi") || strstr(pSource, "usbdev") || strstr(pSource, "dwroot"))
        severity = USPI_LOG_SEVERITY;

    if (severity > __logSeverity)
        return;

    char buf[15 * 80];
    va_list args;

    va_start(args, fmt);
    ee_vsprintf(buf, fmt, args);
    va_end(args);

    LOG_WRITE_STRING("[");
    switch (severity) {
    case LOG_ERROR:
        LOG_WRITE_STRING("ERROR  ");
        break;
    case LOG_WARNING:
        LOG_WRITE_STRING("WARNING");
        break;
    case LOG_NOTICE:
        LOG_WRITE_STRING("NOTICE ");
        break;
    case LOG_DEBUG:
        LOG_WRITE_STRING("DEBUG  ");
        break;
    case LOG_VERBOSE:
        LOG_WRITE_STRING("VERBOSE");
        break;
    default:
        LOG_WRITE_STRING("??     ");
    }

    LOG_WRITE_STRING("] ");
    LOG_WRITE_STRING(pSource);
    LOG_WRITE_STRING(": ");
    LOG_WRITE_STRING(buf);
    LOG_WRITE_STRING("\n");
}

void LogSetLevel(int severity)
{
    __logSeverity = severity;
}

void LogPrintf(const char* fmt, ...)
{
    char buf[15 * 80];
    va_list args;

    va_start(args, fmt);
    ee_vsprintf(buf, fmt, args);
    va_end(args);

    DISP_WRITE_STRING(buf);
}

void LogDumpMemory(unsigned char* start_addr, unsigned char* end_addr)
{
    unsigned char* pAddr = start_addr;
    int linPos = 0;
    for (long i = 0; i < end_addr - start_addr; i++) {
        LogPrintf("%02x", *pAddr++);
        linPos++;
        if (linPos == 16)
        {
            LogPrintf("\r\n");
            linPos = 0;
        }
        else
        {
            LogPrintf(" ");
        }
    }
    LogPrintf("\r\n");
}
