#pragma once

#include <circle/logger.h>

// Severity (change this before building if you want different values)
#define LOG_ERROR	1
#define LOG_WARNING	2
#define LOG_NOTICE	3
#define LOG_DEBUG	4

extern void LogWrite (const char *pSource,		// short name of module
	       unsigned	   severity,		// see above
	       const char *fmt, ...);	// uses printf format options
