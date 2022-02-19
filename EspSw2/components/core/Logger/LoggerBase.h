/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base class for loggers
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdarg.h>
#include "esp_attr.h"
#include "esp_log.h"
#include "ConfigBase.h"

class LoggerBase
{
public:
    LoggerBase(const ConfigBase& config)
    {
        _loggerType = config.getString("type", "");
        _level = convStrToLogLevel(config.getString("level", "").c_str()) ;
    }
    virtual ~LoggerBase()
    {
    }
    virtual void IRAM_ATTR log(esp_log_level_t level, const char *tag, const char* msg) = 0;

    const char* getLoggerType()
    {
        return _loggerType.c_str();
    }

    // Debug
    const char* getLevelStr()
    {
        switch(_level)
        {
        case ESP_LOG_NONE:
            return "NONE";
        case ESP_LOG_ERROR:
            return "ERROR";
        case ESP_LOG_WARN:
            return "WARN";
        case ESP_LOG_INFO:
            return "INFO";
        case ESP_LOG_DEBUG:
            return "DEBUG";
        case ESP_LOG_VERBOSE:
            return "VERBOSE";
        default:
            return "UNKNOWN";
        }
    }

protected:
    String _loggerType;
    esp_log_level_t _level;
    esp_log_level_t convStrToLogLevel(const char* pStr)
    {
        if (pStr == NULL || strlen(pStr) == 0)
            return ESP_LOG_INFO;
        switch (toupper(pStr[0]))
        {
        case 'V':
            return ESP_LOG_VERBOSE;
        case 'D':
            return ESP_LOG_DEBUG;
        case 'I':
            return ESP_LOG_INFO;
        case 'W':
            return ESP_LOG_WARN;
        case 'E':
            return ESP_LOG_ERROR;
        default:
            return ESP_LOG_NONE;
        }
    }
};
