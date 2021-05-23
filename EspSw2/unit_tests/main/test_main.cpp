/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit Testing for RIC Firmware
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "ArduinoTime.h"

// Main task parameters
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
static const int PRO_TASK_PROCESSOR_CORE = 0;
static const int MAIN_TASK_PROCESSOR_CORE = 1;

#include "driver/uart.h"

static void print_banner(const char* text);

extern "C" void app_main(void)
{
    /* These are the different ways of running registered tests.
     * In practice, only one of them is usually needed.
     *
     * UNITY_BEGIN() and UNITY_END() calls tell Unity to print a summary
     * (number of tests executed/failed/ignored) of tests executed between these calls.
     */
    // print_banner("Executing one test by its name");
    // UNITY_BEGIN();
    // unity_run_test_by_name("Mean of an empty array is zero");
    // UNITY_END();

    // print_banner("Running tests with [mean] tag");
    // UNITY_BEGIN();
    // unity_run_tests_by_tag("[mean]", false);
    // UNITY_END();

    // print_banner("Running tests without [fails] tag");
    // UNITY_BEGIN();
    // unity_run_tests_by_tag("[fails]", true);
    // UNITY_END();

    // Disable the watchdog timer on the idle task of both cores
    TaskHandle_t idleTaskOnMainTaskCore = xTaskGetIdleTaskHandleForCPU(MAIN_TASK_PROCESSOR_CORE);
    if (idleTaskOnMainTaskCore)
        esp_task_wdt_delete(idleTaskOnMainTaskCore);
    TaskHandle_t idleTaskOnProCore = xTaskGetIdleTaskHandleForCPU(PRO_TASK_PROCESSOR_CORE);
    if (idleTaskOnProCore)
        esp_task_wdt_delete(idleTaskOnProCore);

    // Setup UART buffered IO with event queue
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)0, uart_buffer_size, \
                                            uart_buffer_size, 10, &uart_queue, 0));
    uart_set_baudrate((uart_port_t)0, 115200);
                                            
    print_banner("Running all the registered tests");
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();

    print_banner("Starting interactive test menu");
    /* This function will not return, and will be busy waiting for UART input.
     * Make sure that task watchdog is disabled if you use this function.
     */
    unity_run_menu();
}

static void print_banner(const char* text)
{
    printf("\n#### %s #####\n\n", text);
}
