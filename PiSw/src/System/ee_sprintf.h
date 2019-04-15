#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int ee_sprintf(char* outBuf, const char* fmt, ...);
extern int ee_vsprintf(char* buf, const char* fmt, va_list args);

#ifdef __cplusplus
}
#endif
