// // Bus Raider
// // Rob Dobson 2019

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
// #include "lowlib.h"
#include "logging.h"
#include <circle/logger.h>
#include <circle/string.h>

// Set output destination
LogOutStrFnType* __log_pOutStrFunction = NULL;
void LogSetOutFn(LogOutStrFnType* pOutFn)
{
    __log_pOutStrFunction = pOutFn;
}

// // Set output to be message based
// LogOutMsgFnType* __log_pOutMsgFunction = NULL;
// void LogSetOutMsgFn(LogOutMsgFnType* pOutFn)
// {
//     __log_pOutMsgFunction = pOutFn;
// }

// // Macros for output
// #define LOG_WRITE_STRING(x) if(__log_pOutStrFunction) (*__log_pOutStrFunction)((const char*)x)

// // Log severity
// #define USPI_LOG_SEVERITY LOG_VERBOSE
// unsigned int __logSeverity = LOG_DEBUG;

unsigned LogSeverityCircle(unsigned severity)
{
    switch(severity)
    {
        case LOG_PANIC: return LogPanic;
        case LOG_ERROR: return LogError;
        case LOG_WARNING: return LogWarning;
        case LOG_NOTICE: return LogNotice;
        case LOG_DEBUG: 
        default: 
            return LogDebug;
    }
}

const char* LogSeverityStr(unsigned severity)
{
    switch(severity)
    {
        case LOG_PANIC: return "P";
        case LOG_ERROR: return "E";
        case LOG_WARNING: return "W";
        case LOG_NOTICE: return "I";
        case LOG_DEBUG: 
        default: 
            return "D";
    }
}

// Main logging function
void LogWrite(const char* pSource,
    unsigned severity,
    const char* fmt, ...)
{

//     // Check for USPI log messages
//     if (strstr(pSource, "uspi") || strstr(pSource, "usbdev") || strstr(pSource, "dwroot"))
//         severity = USPI_LOG_SEVERITY;

//     if (severity > __logSeverity)
//         return;

	va_list var;
	va_start (var, fmt);
	CString logMsg;
	logMsg.FormatV (fmt, var);
	va_end (var);

    // Default to CLogger
    if (!__log_pOutStrFunction)
    {
        CLogger::Get()->Write(pSource, (TLogSeverity)LogSeverityCircle(severity), (const char*)logMsg);
        return;
    }

    // Send to output function
    __log_pOutStrFunction(pSource, LogSeverityStr(severity), (const char*)logMsg);   

//     // Handle message based output
//     if (__log_pOutMsgFunction)
//     {
//         char severityStr[2] = "?";
//         switch (severity) 
//         {
//             case LOG_ERROR: severityStr[0] = 'E'; break;
//             case LOG_WARNING: severityStr[0] = 'W'; break;
//             case LOG_NOTICE: severityStr[0] = 'N'; break;
//             case LOG_DEBUG: severityStr[0] = 'D'; break;
//             case LOG_VERBOSE: severityStr[0] = 'V'; break;
//         }
//         __log_pOutMsgFunction(severityStr, pSource, buf);
//     }

//     // Character based output
//     if (__log_pOutStrFunction)
//     {
//         LOG_WRITE_STRING("[");
//         switch (severity) {
//         case LOG_ERROR:
//             LOG_WRITE_STRING("ERROR  ");
//             break;
//         case LOG_WARNING:
//             LOG_WRITE_STRING("WARNING");
//             break;
//         case LOG_NOTICE:
//             LOG_WRITE_STRING("NOTICE ");
//             break;
//         case LOG_DEBUG:
//             LOG_WRITE_STRING("DEBUG  ");
//             break;
//         case LOG_VERBOSE:
//             LOG_WRITE_STRING("VERBOSE");
//             break;
//         default:
//             LOG_WRITE_STRING("??     ");
//         }

//         LOG_WRITE_STRING("] ");
//         LOG_WRITE_STRING(pSource);
//         LOG_WRITE_STRING(": ");
//         LOG_WRITE_STRING(buf);
//         LOG_WRITE_STRING("\n");
//     }
}

// void LogSetLevel(int severity)
// {
//     __logSeverity = severity;
// }

// void LogDumpMemory(const char* fromSource, int logLevel, unsigned char* start_addr, unsigned char* end_addr)
// {
//     unsigned char* pAddr = start_addr;
//     int linPos = 0;
//     static const int MAX_BUF_LEN = 200;
//     char outLine[MAX_BUF_LEN];
//     strcpy(outLine, "");
//     char tmpBuf[10];
//     for (long i = 0; i < end_addr - start_addr; i++) {
//         linPos++;
//         snprintf(tmpBuf, sizeof(tmpBuf), "%02x", *pAddr++);
//         strlcat(outLine, tmpBuf, MAX_BUF_LEN);
//         if (linPos == 16)
//         {
//             linPos = 0;
//             LogWrite(fromSource, logLevel, outLine);
//             strcpy(outLine, "");
//         }
//     }
//     if (linPos != 0)
//         LogWrite(fromSource, logLevel, outLine);
// }
