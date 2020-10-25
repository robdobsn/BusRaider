// Bus Raider
// Rob Dobson 2019

#pragma once

#include <stdarg.h>

// Severity
#define LOG_PANIC   0
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_NOTICE	3
#define LOG_DEBUG	4
#define LOG_VERBOSE 4

// #define LogWrite(tag, severity, format, ... ) CLogger::Get()->Write(tag, (TLogSeverity)severity, format, ##__VA_ARGS__ )
// // extern void ee_dump_mem(unsigned char* start_addr, unsigned char* end_addr);

extern "C" unsigned LogSeverityCircle(unsigned severity);
extern "C" const char* LogSeverityStr(unsigned severity);

// extern void LogSetLevel(int severity);
extern "C" void LogWrite(const char* pSource,
    unsigned Severity,
    const char* pMessage, ...);

typedef void LogOutStrFnType(const char *pSource, const char* severityStr, const char *pMessage);
extern "C" void LogSetOutFn(LogOutStrFnType* pOutFn);

// typedef void LogOutMsgFnType(const char* pSeverity, const char* pSource, const char* pMsg);
// extern void LogSetOutMsgFn(LogOutMsgFnType* pOutFn);

// extern void LogDumpMemory(const char* fromSource, int logLevel, unsigned char* start_addr, unsigned char* end_addr);

