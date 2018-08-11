#ifndef _EE_PRINTF_H_
#define _EE_PRINTF_H_

// Severity
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_NOTICE	3
#define LOG_DEBUG	4

extern void ee_printf(const char* fmt, ...);
extern void uart_printf(const char* fmt, ...);

extern void LogSetLevel(int severity);
extern void LogWrite(const char* pSource, // short name of module
    unsigned Severity, // see above
    const char* pMessage, ...); // uses printf format options

#endif
