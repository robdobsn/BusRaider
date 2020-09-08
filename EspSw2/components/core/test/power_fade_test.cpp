/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Power Fade Test - NOT A UNIT TEST so shouldn't generally be included in CMakeLists.txt
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <limits.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include <stdio.h>

TEST_CASE("Expression test 1", "[expressions]")
{
    #define LEDC_HS_TIMER          LEDC_TIMER_0
    #define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
    #define LEDC_HS_CH0_GPIO       (2)
    #define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0

    #define LEDC_TEST_DUTY         (0x1fff)
    #define LEDC_TEST_FADE_TIME    (10000)

    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HS_MODE,           // timer mode
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .timer_num = LEDC_HS_TIMER,            // timer index
        .freq_hz = 5000,                      // frequency of PWM signal
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
#pragma GCC diagnostic pop

    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    /*
     * Prepare individual configuration
     * for each channel of LED Controller
     * by selecting:
     * - controller's channel number
     * - output duty cycle, set initially to 0
     * - GPIO number where LED is connected to
     * - speed mode, either high or low
     * - timer servicing selected channel
     *   Note: if different channels use one timer,
     *         then frequency and bit_num of these channels
     *         will be the same
     */

    ledc_channel_config_t ledc_channel = {
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .channel    = LEDC_HS_CH0_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_HS_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };

    // Set LED Controller with previously prepared configuration
    ledc_channel_config(&ledc_channel);

    // Initialize fade service.
    ledc_fade_func_install(0);

    while (1) {
        printf("1. LEDC fade up to duty = %d\n", LEDC_TEST_DUTY);
        ledc_set_fade_with_time(ledc_channel.speed_mode,
                ledc_channel.channel, LEDC_TEST_DUTY+1, 3000);
        ledc_fade_start(ledc_channel.speed_mode,
                ledc_channel.channel, LEDC_FADE_NO_WAIT);
        vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);

        printf("2. LEDC fade down to duty = 0\n");
        ledc_set_fade_with_time(ledc_channel.speed_mode,
                ledc_channel.channel, 0, 1000);
        ledc_fade_start(ledc_channel.speed_mode,
                ledc_channel.channel, LEDC_FADE_NO_WAIT);
        vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);

        // printf("3. LEDC set duty = %d without fade\n", LEDC_TEST_DUTY);
        // ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, LEDC_TEST_DUTY);
        // ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);

        // printf("4. LEDC set duty = 0 without fade\n");
        // ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
        // ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
