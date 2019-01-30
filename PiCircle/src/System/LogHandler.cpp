#include "LogHandler.h"

void LogWrite (const char *pSource,		// short name of module
	       unsigned	   severity,		// see above
	       const char *fmt, ...)	// uses printf format options
{
    if (!CLogger::Get())
        return;

    TLogSeverity logSeverity = LogError;
    switch (severity) {
        case LOG_ERROR:
        default:
            break;
        case LOG_WARNING:
            logSeverity = LogWarning;
            break;
        case LOG_NOTICE:
            logSeverity = LogNotice;
            break;
        case LOG_DEBUG:
            logSeverity = LogDebug;
            break;
    }

    va_list var;
    va_start(var, fmt);
    CLogger::Get()->WriteV(pSource, logSeverity, fmt, var);
    va_end(var);
}

