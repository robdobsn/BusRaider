// ESPUtils
// Rob Dobson 2020

#pragma once

#include <cstdint>
#include <WString.h>
#include "esp_system.h"

// enable/disable WDT for the IDLE task on Core 0 (SYSTEM)
void enableCore0WDT();
void disableCore0WDT();

// enable/disable WDT for the IDLE task on Core 1 (Arduino)
void enableCore1WDT();
void disableCore1WDT();

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html
// WIFI STA is base MAC address
// WIFI SoftAP is base + 1
// BT is base +2
// Ethernet is base +3
String getSystemMACAddressStr(esp_mac_type_t macType, const char* pSeparator);
