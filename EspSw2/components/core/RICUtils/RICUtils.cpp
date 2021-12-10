// RICUtils
// Rob Dobson 2020

#include <RICUtils.h>
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
static const char* MODULE_PREFIX = "RICUtils";
#endif

int getRICRevision()
{
    static int hwRev = HW_REVISION_NUMBER_UNKNOWN;

    // Since hwRev is static detection should run exactly once and thereafter
    // return the detected revision info
    if (hwRev == HW_REVISION_NUMBER_UNKNOWN)
    {

#ifdef RIC_FORCE_HW_REVISION_NUMBER
        hwRev = RIC_FORCE_HW_REVISION_NUMBER;
#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
        LOG_I(MODULE_PREFIX, "getRICRevision revision forced RICHwRev %d", hwRev);
#endif
#else
        // Check chip revision
        esp_chip_info_t chipInfo;
        esp_chip_info(&chipInfo);
        if (chipInfo.revision < 3)
        {
            hwRev = HW_REVISION_NUMBER_1;
#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
            LOG_I(MODULE_PREFIX, "getRICRevision ESP32 model %d features %08x cores %d esp32Rev %d RICHwRev %d", 
                    chipInfo.model, chipInfo.features, chipInfo.cores, chipInfo.revision, hwRev);
#endif
        }
        else
        {

            // Detection on RIC Rev4+
            static const uint32_t REV_POTENTIAL_DIVIDER_PIN = 32;
            static const uint16_t EXPECTED_REV4_ADC_LOW_VALUE = 1800;
            static const uint16_t EXPECTED_REV4_ADC_HIGH_VALUE = 2000;

            // Check for potential-divider on Rev4 (ensure the level is not floating)
            pinMode(REV_POTENTIAL_DIVIDER_PIN, INPUT_PULLUP);
            delay(1);
            uint16_t adcVal1 = analogRead(REV_POTENTIAL_DIVIDER_PIN);
            pinMode(REV_POTENTIAL_DIVIDER_PIN, INPUT_PULLDOWN);
            delay(1);
            uint16_t adcVal2 = analogRead(REV_POTENTIAL_DIVIDER_PIN);

            // Check thresholds
            if ((adcVal1 < EXPECTED_REV4_ADC_HIGH_VALUE) && (adcVal1 > EXPECTED_REV4_ADC_LOW_VALUE) &&
                (adcVal2 < EXPECTED_REV4_ADC_HIGH_VALUE) && (adcVal2 > EXPECTED_REV4_ADC_LOW_VALUE))
            {
                hwRev = HW_REVISION_NUMBER_4;
            }
            else
            {
                hwRev = HW_REVISION_NUMBER_2_OR_3;
            }

            // Restore test pin
            pinMode(REV_POTENTIAL_DIVIDER_PIN, INPUT);

#ifdef DEBUG_HW_REVISION_NUMBER_DETECTION
            // Debug
            LOG_I(MODULE_PREFIX, "getRICRevision ESP32 model %d features %08x cores %d esp32Rev %d adc1 %d adc2 %d RICHwRev %d", 
                    chipInfo.model, chipInfo.features, chipInfo.cores, chipInfo.revision, adcVal1, adcVal2, hwRev);
#endif
        }
#endif // RIC_FORCE_HW_REVISION_NUMBER
    }
    return hwRev;
}