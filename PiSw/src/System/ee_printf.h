#pragma once

// Severity
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_NOTICE	3
#define LOG_DEBUG	4

#ifdef __cplusplus
extern "C" {
#endif

extern void ee_printf(const char* fmt, ...);
extern int ee_sprintf(char* outBuf, const char* fmt, ...);
extern void uart_printf(const char* fmt, ...);

extern void LogSetLevel(int severity);
extern void LogWrite(const char* pSource, // short name of module
    unsigned Severity, // see above
    const char* pMessage, ...); // uses printf format options

#ifdef __cplusplus
}
#endif
