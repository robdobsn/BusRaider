// Bus Raider
// Rob Dobson 2019

#pragma once

// Severity
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_NOTICE	3
#define LOG_DEBUG	4
#define LOG_VERBOSE 5

#ifdef __cplusplus
extern "C" {
#endif

// extern void ee_dump_mem(unsigned char* start_addr, unsigned char* end_addr);

extern void LogSetLevel(int severity);
extern void LogWrite(const char* pSource, // short name of module
    unsigned Severity, // see above
    const char* pMessage, ...); // uses printf format options

typedef void LogOutChFnType(const char* pStr);
extern void LogSetOutFn(LogOutChFnType* pOutFn);

typedef void LogOutMsgFnType(const char* pSeverity, const char* pSource, const char* pMsg);
extern void LogSetOutMsgFn(LogOutMsgFnType* pOutFn);

extern void LogDumpMemory(unsigned char* start_addr, unsigned char* end_addr);

extern void LogPrintf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif
