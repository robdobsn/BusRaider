/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LoggerCore
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LoggerCore.h"
#include "ArduinoTime.h"
#include <SpiramAwareAllocator.h>

LoggerCore loggerCore;

#include <stdio.h>

void IRAM_ATTR loggerLog(esp_log_level_t level, const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    static const uint32_t MAX_VSPRINTF_BUFFER_SIZE = 1024;
    std::vector<char, SpiramAwareAllocator<char>> buf(MAX_VSPRINTF_BUFFER_SIZE);
    vsnprintf(buf.data(), buf.size(), format, args);
    loggerCore.log(level, tag, buf.data());
    va_end(args);
}

LoggerCore::LoggerCore()
{

}

LoggerCore::~LoggerCore()
{
    for (LoggerBase* pLogger : _loggers)
    {
        delete pLogger;
    }
}

void LoggerCore::clearLoggers()
{
    _loggers.clear();
}

void LoggerCore::addLogger(LoggerBase* pLogger)
{
    _loggers.push_back(pLogger);
}

void IRAM_ATTR LoggerCore::log(esp_log_level_t level, const char *tag, const char *msg)
{
    printf(msg);

    // Log to all loggers
    for (LoggerBase* pLogger : _loggers)
    {
        // printf("LoggerCore::log type %s msg %s", pLogger->getLoggerType(), msg);
        pLogger->log(level, tag, msg);
    }
    // va_list args;
    // va_start(args, format);
    // log(pModuleName, pFormat, args);
    // char buf[MAX_VSPRINTF_BUFFER_SIZE];
    // vsnprintf(buf, MAX_VSPRINTF_BUFFER_SIZE, format, args);
    // ESP_LOG_LEVEL_LOCAL(level, tag, format, args);
    // ESP_LOG_LEVEL_LOCAL(level, tag, "this %d", 1);
    // esp_log_write(level, tag, LOGGER_CORE_FORMAT(format), esp_log_timestamp(), tag, args);
    // va_end(args);
}


// void LOG_E(const char *tag, const char *format, ...)
// {
//     va_list args;
//     va_start(args, format);
//     loggerCore.log(ESP_LOG_ERROR, tag, format, args);
//     va_end(args);
// }
// void LOG_W(const char *tag, const char *format, ...)
// {
//     va_list args;
//     va_start(args, format);
//     loggerCore.log(ESP_LOG_WARN, tag, format, args);
//     va_end(args);
// }
// void LOG_I(const char *tag, const char *format, ...)
// {
//     va_list args;
//     va_start(args, format);
//     loggerCore.log(ESP_LOG_INFO, tag, format, args);
//     va_end(args);
// }
// void LOG_D(const char *tag, const char *format, ...)
// {
//     va_list args;
//     va_start(args, format);
//     loggerCore.log(ESP_LOG_DEBUG, tag, format, args);
//     va_end(args);
// }
// void LOG_V(const char *tag, const char *format, ...)
// {
//     va_list args;
//     va_start(args, format);
//     loggerCore.log(ESP_LOG_VERBOSE, tag, format, args);
//     va_end(args);
// }
