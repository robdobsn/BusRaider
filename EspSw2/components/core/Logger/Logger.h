/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Log
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "esp_log.h"
#include "esp_attr.h"

#define LOG_E( tag, format, ... ) ESP_LOGE( tag, format, ##__VA_ARGS__ )
#define LOG_W( tag, format, ... ) ESP_LOGW( tag, format, ##__VA_ARGS__ )
#define LOG_I( tag, format, ... ) ESP_LOGI( tag, format, ##__VA_ARGS__ )
#define LOG_D( tag, format, ... ) ESP_LOGD( tag, format, ##__VA_ARGS__ )
#define LOG_V( tag, format, ... ) ESP_LOGV( tag, format, ##__VA_ARGS__ )

#define DEBUG_GLOB_SYSMAN 0
#define DEBUG_GLOB_ROBOTCTRL 1
#define DEBUG_GLOB_HWMAN 2

#define DEBUG_GLOBAL_VALUE
#ifdef DEBUG_GLOBAL_VALUE
extern volatile int32_t __loggerGlobalDebugValue0;
extern volatile int32_t __loggerGlobalDebugValue1;
extern volatile int32_t __loggerGlobalDebugValue2;
extern volatile int32_t __loggerGlobalDebugValue3;
extern volatile int32_t __loggerGlobalDebugValue4;
#define DEBUG_GLOB_VAR_NAME(x) __loggerGlobalDebugValue ## x
#endif

// class Logger
// {
// public:
//     Logger()
//     {}

//     void begin(esp_log_level_t level)
//     {
//         esp_log_level_set("*", level);
//     }

//    template <class T, typename... Args>
//     void IRAM_ATTR error2(const char* tag, T format, Args... args)
//     {
//         esp_log_write(ESP_LOG_ERROR, tag, LOG_FORMAT(E, format), esp_log_timestamp(), tag, args...);
//         // ESP_LOGE("this", "that", args...);
//     }

//    template <class T, typename... Args>
//     void IRAM_ATTR error(const char* tag, T format, Args... args)
//     {
// #ifndef DISABLE_LOGGING
//         esp_log_write(ESP_LOG_ERROR, tag, format, args...);
// #endif
//     }

//     template <class T, typename... Args>
//     void IRAM_ATTR warning(const char* tag, T format, Args... args)
//     {
// #ifndef DISABLE_LOGGING
//         esp_log_write(ESP_LOG_WARN, tag, format, args...);
// #endif
//     }

//     template <class T, typename... Args>
//     void IRAM_ATTR notice(const char* tag, T format, Args... args)
//     {
// #ifndef DISABLE_LOGGING
//         esp_log_write(ESP_LOG_INFO, tag, format, args...);
// #endif
//     }

//     template <class T, typename... Args>
//     void IRAM_ATTR trace(const char* tag, T format, Args... args)
//     {
// #ifndef DISABLE_LOGGING
//         esp_log_write(ESP_LOG_DEBUG, tag, format, args...);
// #endif
//     }

//     template <class T, typename... Args>
//     void IRAM_ATTR verbose(const char* tag, T msg, Args... args)
//     {
// #ifndef DISABLE_LOGGING
//         esp_log_write(ESP_LOG_VERBOSE, tag, msg, args...);
// #endif
//     }
// };

// extern Logger Log;
