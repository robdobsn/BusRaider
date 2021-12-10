/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Debug Globals
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <ArduinoTime.h>

#define DEBUG_GLOBAL_VALUE
#ifdef DEBUG_GLOBAL_VALUE
extern volatile int32_t __loggerGlobalDebugValue0;
extern volatile int32_t __loggerGlobalDebugValue1;
extern volatile int32_t __loggerGlobalDebugValue2;
extern volatile int32_t __loggerGlobalDebugValue3;
extern volatile int32_t __loggerGlobalDebugValue4;
#define DEBUG_GLOB_VAR_NAME(x) __loggerGlobalDebugValue ## x
#endif

class DebugGlobals
{
public:
    static String getDebugJson(bool includeOuterBrackets)
    {
        char outStr[100];
        snprintf(outStr, sizeof(outStr), "[%d,%d,%d,%d,%d]", 
            __loggerGlobalDebugValue0,
            __loggerGlobalDebugValue1,
            __loggerGlobalDebugValue2,
            __loggerGlobalDebugValue3,
            __loggerGlobalDebugValue4);
        if (includeOuterBrackets)
            return R"({"globs":)" + String(outStr) + "}";
        return outStr;
    }
};