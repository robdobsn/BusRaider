/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ESPUtils
//
// Rob Dobson 2020-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ESP8266

#include <ESPUtils.h>
#include <limits.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "sdkconfig.h"
#ifdef CONFIG_ESP32_SPIRAM_SUPPORT
#ifdef __cplusplus
extern "C" {
#endif
#include "esp32/spiram.h"
#ifdef __cplusplus
}
#endif
#endif

void enableCore0WDT(){
    TaskHandle_t idle_0 = xTaskGetIdleTaskHandleForCPU(0);
    if(idle_0 == NULL || esp_task_wdt_add(idle_0) != ESP_OK){
        esp_log_write(ESP_LOG_ERROR, "", "Failed to add Core 0 IDLE task to WDT");
    }
}

void disableCore0WDT(){
    TaskHandle_t idle_0 = xTaskGetIdleTaskHandleForCPU(0);
    if(idle_0 == NULL || esp_task_wdt_delete(idle_0) != ESP_OK){
        esp_log_write(ESP_LOG_ERROR, "", "Failed to remove Core 0 IDLE task from WDT");
    }
}

void enableCore1WDT(){
    TaskHandle_t idle_1 = xTaskGetIdleTaskHandleForCPU(1);
    if(idle_1 == NULL || esp_task_wdt_add(idle_1) != ESP_OK){
        esp_log_write(ESP_LOG_ERROR, "", "Failed to add Core 1 IDLE task to WDT");
    }
}

void disableCore1WDT(){
    TaskHandle_t idle_1 = xTaskGetIdleTaskHandleForCPU(1);
    if(idle_1 == NULL || esp_task_wdt_delete(idle_1) != ESP_OK){
        esp_log_write(ESP_LOG_ERROR, "", "Failed to remove Core 1 IDLE task from WDT");
    }
}

static String __systemMACAddrBT;
static String __systemMACAddrETH;
static String __systemMACAddrSTA;
String getSystemMACAddressStr(esp_mac_type_t macType, const char* pSeparator)
{
    // Check if already got
    if ((macType == ESP_MAC_BT) && (__systemMACAddrBT.length() > 0))
        return __systemMACAddrBT;
    else if ((macType == ESP_MAC_ETH) && (__systemMACAddrETH.length() > 0))
        return __systemMACAddrETH;
    else if ((macType == ESP_MAC_WIFI_STA) && (__systemMACAddrSTA.length() > 0))
        return __systemMACAddrSTA;

    // Use the public (MAC) address of BLE
    uint8_t addr[6] = {0,0,0,0,0,0};
    int rc = esp_read_mac(addr, macType);
    if (rc != ESP_OK)
        return "";
    char outStr[50];
    snprintf(outStr, sizeof(outStr), "%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
            addr[0], pSeparator, addr[1], pSeparator, addr[2], pSeparator,
            addr[3], pSeparator, addr[4], pSeparator, addr[5]);
    if (macType == ESP_MAC_BT)
        __systemMACAddrBT = outStr;
    else if (macType == ESP_MAC_ETH)
        __systemMACAddrETH = outStr;
    else if (macType == ESP_MAC_WIFI_STA)
        __systemMACAddrSTA = outStr;
    return outStr;
}

// Get size of SPIRAM (returns 0 if not available)
uint32_t utilsGetSPIRAMSize()
{
#ifdef CONFIG_ESP32_SPIRAM_SUPPORT
    switch (esp_spiram_get_chip_size())
    {
        case ESP_SPIRAM_SIZE_16MBITS:
            return 2 * 1024 * 1024;
        case ESP_SPIRAM_SIZE_32MBITS:
        case ESP_SPIRAM_SIZE_64MBITS:
            // Note that only 4MBytes is usable
            return 4 * 1024 * 1024;
        default:
            return 0;
    }
#else
    return 0;
#endif
}

#endif