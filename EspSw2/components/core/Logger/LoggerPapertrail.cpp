/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Papertrail logger
//
// Rob Dobson 2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LoggerPapertrail.h"
#include <ConfigBase.h>
#include <Logger.h>
#include <NetworkSystem.h>
#include <ESPUtils.h>

// Log prefix
static const char *MODULE_PREFIX = "LogPapertrail";

LoggerPapertrail::LoggerPapertrail(const ConfigBase& logDestConfig)
    : LoggerBase(logDestConfig)
{
    // Get config
    _host = logDestConfig.getString("host", "");
    _port = logDestConfig.getString("port", "");
    memset(&_hostAddrInfo, 0, sizeof(struct addrinfo));
    _dnsLookupDone = false;
    _port = logDestConfig.getLong("port", 0);
    _sysName = logDestConfig.getString("sysName", "");
    _sysName += "_" + getSystemMACAddressStr(ESP_MAC_WIFI_STA, ":");

    // Debug
    ESP_LOGI(MODULE_PREFIX, "host %s port %s level %s sysName %s", _host.c_str(), _port.c_str(), getLevelStr(), _sysName.c_str());
}

LoggerPapertrail::~LoggerPapertrail()
{
}

void IRAM_ATTR LoggerPapertrail::log(esp_log_level_t level, const char *tag, const char* msg)
{
    if (level <= _level)
    {
        // Check if we're connected
        if (networkSystem.isWiFiStaConnectedWithIP())
        {
            // Check if DNS lookup done
            if (!_dnsLookupDone)
            {
                struct addrinfo hints;
                memset(&hints,0,sizeof(hints));
                hints.ai_family=AF_INET;
                hints.ai_socktype=SOCK_DGRAM;
                hints.ai_flags=0;
                struct addrinfo *addrResult;
                if (getaddrinfo(_host.c_str(), _port.c_str(), &hints, &addrResult) != 0)
                {
                    ESP_LOGE(MODULE_PREFIX, "failed to resolve host %s", _host.c_str());
                    return;
                }
                ESP_LOGI(MODULE_PREFIX, "resolved host %s to %d.%d.%d.%d", _host.c_str(), 
                            addrResult->ai_addr->sa_data[0], addrResult->ai_addr->sa_data[1],
                            addrResult->ai_addr->sa_data[2], addrResult->ai_addr->sa_data[3]);
                _hostAddrInfo = *addrResult;
                _dnsLookupDone = true;
            }
            
            // Format message
            String logMsg = "<22>" + _sysName + ": " + msg;

            // Create UDP socket
            int socketFileDesc = socket(_hostAddrInfo.ai_family, _hostAddrInfo.ai_socktype, _hostAddrInfo.ai_protocol);
            if (socketFileDesc < 0)
            {
                ESP_LOGE(MODULE_PREFIX, "create udp socket failed: %d", socketFileDesc);
                return;
            }

            // Send to papertrail using UDP socket
            int ret = sendto(socketFileDesc, logMsg.c_str(), logMsg.length(), 0, _hostAddrInfo.ai_addr, _hostAddrInfo.ai_addrlen);
            if (ret < 0)
            {
                ESP_LOGE(MODULE_PREFIX, "log failed: %d", ret);
                return;
            }

            // Close socket
            close(socketFileDesc);
        }
    }
}
