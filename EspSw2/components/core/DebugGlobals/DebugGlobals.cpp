/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DebugGlobals
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <WString.h>
#include "DebugGlobals.h"

#ifdef DEBUG_GLOBAL_VALUE
volatile int32_t __loggerGlobalDebugValue0 = -1;
volatile int32_t __loggerGlobalDebugValue1 = -1;
volatile int32_t __loggerGlobalDebugValue2 = -1;
volatile int32_t __loggerGlobalDebugValue3 = -1;
volatile int32_t __loggerGlobalDebugValue4 = -1;
#endif

