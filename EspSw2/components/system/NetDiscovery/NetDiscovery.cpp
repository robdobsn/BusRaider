/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Net Discovery
// Enables discovery
//
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Logger.h>
#include "NetDiscovery.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

// TODO
// This is incomplete implementation of something that was in the original Marty1 firmware but may no
// longer be relevant - especially as many networks block multicasting

// Log prefix
static const char *MODULE_PREFIX = "NetDiscovery";

// Statics
uint32_t NetDiscovery::_udbBroadcastPort = NetDiscovery::DEFAULT_UDP_BROADCAST_PORT;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NetDiscovery::NetDiscovery(const char *pModuleName, ConfigBase &defaultConfig, ConfigBase *pGlobalConfig, ConfigBase *pMutableConfig)
    : SysModBase(pModuleName, defaultConfig, pGlobalConfig, pMutableConfig)
{
    _discoveryTaskHandle = NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetDiscovery::setup()
{
    applySetup();
}

void NetDiscovery::applySetup()
{
    // Extract info from config
    _isDiscoveryEnabled = (configGetLong("enable", 0) != 0);
    _udbBroadcastPort = configGetLong("udpPort", DEFAULT_UDP_BROADCAST_PORT);

    // Create task if required
    if (_isDiscoveryEnabled && (_discoveryTaskHandle == NULL))
    {
        xTaskCreate(discoveryUDPTaskWorker, "discoServer", 4096, NULL, 5, &_discoveryTaskHandle);
    }

    // Debug
    LOG_I(MODULE_PREFIX, "setup isEnabled %s udpBroadcastPort %d", 
            _isDiscoveryEnabled ? "YES" : "NO", _udbBroadcastPort);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Service
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetDiscovery::service()
{
#ifndef USE_FREERTOS_TASK
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String NetDiscovery::getStatusJSON()
{
    return "{\"rslt\":\"ok\"}";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get status JSON
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NetDiscovery::discoveryUDPTaskWorker(void* pvParameters)
{
    LOG_I(MODULE_PREFIX, "worker started");
    while (1) 
    {
        // Yield
        vTaskDelay(1);

        // Start UDP
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(_udbBroadcastPort);
        int addr_family = AF_INET;
        int ip_protocol = IPPROTO_IP;
        char addr_str[128];
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        // Socket
        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            LOG_E(MODULE_PREFIX, "Unable to create socket: errno %d", errno);
            continue;
        }
        LOG_I(MODULE_PREFIX, "Socket created");

        // Bind
        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            LOG_E(MODULE_PREFIX, "Socket unable to bind: errno %d", errno);
        }
        LOG_I(MODULE_PREFIX, "Socket bound, port %d", _udbBroadcastPort);

        while (1) {

            LOG_I(MODULE_PREFIX, "Waiting for data");
            struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            char rx_buffer[128];
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                LOG_E(MODULE_PREFIX, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                LOG_I(MODULE_PREFIX, "Received %d bytes from %s:", len, addr_str);
                LOG_I(MODULE_PREFIX, "%s", rx_buffer);

                int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                if (err < 0) {
                    LOG_I(MODULE_PREFIX, "Error occurred during sending: errno %d", errno);
                    break;
                }
            }

            // Yield
            vTaskDelay(1);
        }

        // Check for failure
        if (sock != -1) 
        {
            LOG_E(MODULE_PREFIX, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
}
