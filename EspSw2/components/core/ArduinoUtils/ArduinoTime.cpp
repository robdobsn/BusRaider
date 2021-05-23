// ArduinoTime
// Rob Dobson 2012-2021

#include "stdint.h"
#include "ArduinoTime.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Low-level time functions for Arduino compat
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint64_t IRAM_ATTR micros()
{
    return (uint64_t) (esp_timer_get_time());
}

unsigned long IRAM_ATTR millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

void IRAM_ATTR delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void IRAM_ATTR delayMicroseconds(uint64_t us)
{
    uint64_t m = micros();
    if(us){
        int64_t e = (m + us);
        if(m > e){ //overflow
            while(micros() > e){
                __asm__ volatile ("nop");
            }
        }
        while(micros() < e){
            __asm__ volatile ("nop");
        }
    }
}
