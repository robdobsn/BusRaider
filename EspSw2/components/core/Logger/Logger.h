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
