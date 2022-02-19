/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Papertrail logger
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "LoggerBase.h"
#include <WString.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

class ConfigBase;

class LoggerPapertrail : public LoggerBase
{
public:
    LoggerPapertrail(const ConfigBase& logDestConfig);
    virtual ~LoggerPapertrail();
    virtual void IRAM_ATTR log(esp_log_level_t level, const char *tag, const char* msg) override final;

private:
    String _host;
    String _port;
    String _sysName;
    struct addrinfo _hostAddrInfo;
    bool _dnsLookupDone;
};
