/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// HWUtils
// Get hardware revision info
//
// Rob Dobson 2020-2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <HWUtils.h>
#include <Logger.h>
#include "sdkconfig.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "esp_system.h"
#ifdef __cplusplus
}
#endif

#include "ArduinoGPIO.h"
#include "ArduinoTime.h"

// Debug
#define DEBUG_HW_REVISION_NUMBER_DETECTION
#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
static const char* MODULE_PREFIX = "HWUtils";
#endif

int getHWRevision()
{
    static int hwRev = HW_REVISION_NUMBER_UNKNOWN;

    // Since hwRev is static detection should run exactly once and thereafter
    // return the detected revision info
    if (hwRev != HW_REVISION_NUMBER_UNKNOWN)
        return hwRev;

    // Handle forcing of revision number
#ifdef HW_REVISION_FORCE_TO
    hwRev = HW_REVISION_FORCE_TO;
#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
    LOG_I(MODULE_PREFIX, "getHWRevision revision forced HwRev %d", hwRev);
#endif
    return hwRev;
#endif

    // Check chip revision
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);

    // Check if chip revision is to be used
#if defined(HW_REVISION_ESP32_LIMIT_ESPREV) && defined (HW_REVISION_ESP32_LIMIT_HWREV)
    if (chipInfo.revision >= HW_REVISION_ESP32_LIMIT_ESPREV)
    {
        hwRev = HW_REVISION_ESP32_LIMIT_HWREV;
#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
        LOG_I(MODULE_PREFIX, "getHWRevision ESP32 model %d features %08x cores %d esp32Rev %d HwRev %d", 
                chipInfo.model, chipInfo.features, chipInfo.cores, chipInfo.revision, hwRev);
#endif
        return hwRev;
    }
#endif


    // Handle revision detection using a resistor-ladder on an ADC input
#ifdef HW_REVISION_LADDER_PIN
    // Check for potential-divider (ensure the level is not floating)
    pinMode(HW_REVISION_LADDER_PIN, INPUT_PULLUP);
    delay(1);
    uint16_t adcVal1 = analogRead(HW_REVISION_LADDER_PIN);
    pinMode(HW_REVISION_LADDER_PIN, INPUT_PULLDOWN);
    delay(1);
    uint16_t adcVal2 = analogRead(HW_REVISION_LADDER_PIN);
    // Restore test pin
    pinMode(HW_REVISION_LADDER_PIN, INPUT);

#if defined(HW_REVISION_LOW_THRESH_1) && defined(HW_REVISION_HIGH_THRESH_1) && defined(HW_REVISION_HWREV_THRESH_1)
    // Check thresholds
    if ((adcVal1 < HW_REVISION_HIGH_THRESH_1) && (adcVal1 > HW_REVISION_LOW_THRESH_1) &&
        (adcVal2 < HW_REVISION_HIGH_THRESH_1) && (adcVal2 > HW_REVISION_LOW_THRESH_1))
    {
        hwRev = HW_REVISION_HWREV_THRESH_1;
    }
#endif

#if defined(HW_REVISION_LOW_THRESH_2) && defined(HW_REVISION_HIGH_THRESH_2) && defined(HW_REVISION_HWREV_THRESH_2)
    // Check thresholds
    if ((adcVal1 < HW_REVISION_HIGH_THRESH_2) && (adcVal1 > HW_REVISION_LOW_THRESH_2) &&
        (adcVal2 < HW_REVISION_HIGH_THRESH_2) && (adcVal2 > HW_REVISION_LOW_THRESH_2))
    {
        hwRev = HW_REVISION_HWREV_THRESH_2;
    }
#endif

#if defined(HW_REVISION_LOW_THRESH_3) && defined(HW_REVISION_HIGH_THRESH_3) && defined(HW_REVISION_HWREV_THRESH_3)
    // Check thresholds
    if ((adcVal1 < HW_REVISION_HIGH_THRESH_3) && (adcVal1 > HW_REVISION_LOW_THRESH_3) &&
        (adcVal2 < HW_REVISION_HIGH_THRESH_3) && (adcVal2 > HW_REVISION_LOW_THRESH_3))
    {
        hwRev = HW_REVISION_HWREV_THRESH_3;
    }
#endif

#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
    // Debug
    if (hwRev != HW_REVISION_NUMBER_UNKNOWN)
    {
        LOG_I(MODULE_PREFIX, "getHWRevision ESP32 model %d features %08x cores %d esp32Rev %d adc1 %d adc2 %d HwRev %d", 
                chipInfo.model, chipInfo.features, chipInfo.cores, chipInfo.revision, adcVal1, adcVal2, hwRev);
    }
#endif

#endif // HW_REVISION_LADDER_PIN

    // Ensure default to revision 1
    if (hwRev == HW_REVISION_NUMBER_UNKNOWN)
        hwRev = 1;

#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
    // Debug
    LOG_I(MODULE_PREFIX, "getHWRevision ESP32 model %d features %08x cores %d esp32Rev %d default HwRev %d", 
            chipInfo.model, chipInfo.features, chipInfo.cores, chipInfo.revision, hwRev);
#endif

    return hwRev;
}