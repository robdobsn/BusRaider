/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main entry point
//
// BusRaider ESP32
// Rob Dobson 2018-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Name and Version
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SYSTEM_VERSION "3.1.8"

#define MACRO_STRINGIFY(x) #x
#define MACRO_TOSTRING(x) MACRO_STRINGIFY(x)
#define SYSTEM_NAME MACRO_TOSTRING(RIC_SYSTEM_NAME)
#define DEFAULT_FRIENDLY_NAME MACRO_TOSTRING(RIC_DEFAULT_FRIENDLY_NAME)
#define DEFAULT_HOSTNAME MACRO_TOSTRING(RIC_DEFAULT_HOSTNAME)
#define DEFAULT_ADVNAME MACRO_TOSTRING(RIC_DEFAULT_ADVNAME)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Default system config
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE:
// In VSCode C++ raw strings can be removed - to reveal JSON with regex search and replace:
//      regex:    R"\((.*)\)"
//      replace:  $1
// And then reinserted with:
//      regex:    (\s*)(.*)
//      replace:  $1R"($2)"

static const char *defaultConfigJSON =
    R"({)"
        R"("SystemName":")" SYSTEM_NAME R"(",)"
        R"("SystemVersion":")" SYSTEM_VERSION R"(",)"
        R"("IDFVersion":")" IDF_VER R"(",)"
        R"("DefaultName":")" DEFAULT_FRIENDLY_NAME R"(",)"
        R"("DefaultNameIsSet":0,)"
        R"("SysManager":{)"
            R"("monitorPeriodMs":10000,)"
            R"("reportList":["NetMan","SysMan","StatsCB"],)"
            R"("pauseWiFiforBLE":1,)"
            R"("RICSerial":{)"
                R"("FrameBound":"0xE7",)"
                R"("CtrlEscape":"0xD7")"
            R"(})"
        R"(})"
    R"(})"
;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Parameters
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Main task parameters
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const int MAIN_TASK_PRIORITY = 20;
static const int PRO_TASK_PROCESSOR_CORE = 0;
static const int MAIN_TASK_PROCESSOR_CORE = 1;
static const int MAIN_TASK_STACK_SIZE = 16384;
static TaskHandle_t mainTaskHandle = nullptr;
extern void mainTask(void *pvParameters);

// Debug
static const char* MODULE_NAME = "MainTask";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #define TEST_MINIMAL_FLASH_HEAP_BASE_CASE
// #define DEBUG_SHOW_TASK_INFO
// #define DEBUG_SHOW_RUNTIME_STATS
// #define DEBUG_HEAP_ALLOCATION
// #define DEBUG_TIMING_OF_STARTUP

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#ifdef DEBUG_SHOW_TASK_INFO
#define DEBUG_SHOW_TASK_MS 10000
int tasks_info();
uint32_t lasttaskdumpms = 0;
#endif
#ifdef DEBUG_SHOW_RUNTIME_STATS
#define DEBUG_SHOW_RUNTIME_STATS_MS 10000
int runtime_stats();
uint32_t laststatsdumpms = 0;
#endif
#endif

#ifdef DEBUG_HEAP_ALLOCATION
#include "esp_heap_trace.h"
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Includes
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// C, ESP32 and RTOS
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"

// App
#include <ConfigNVS.h>
#include <SysTypeManager.h>
#include <SysManager.h>
#include <SerialConsole.h>
#include <FileManager.h>
#include <BLEManager.h>
#ifdef FEATURE_WIFI_FUNCTIONALITY
#include <NetworkManager.h>
#include <NetDiscovery.h>
#ifdef FEATURE_WEB_SERVER_OR_WEB_SOCKETS
#include <WebServer.h>
#endif
#endif
#include <CommandSerial.h>
#include <CommandSocket.h>
#include <CommandFile.h>
#include <FileSystem.h>
#include <ESPOTAUpdate.h>
#include <PiCoProcessor.h>

// System type consts
#include "RICSysTypes.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Standard Entry Point
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef TEST_MINIMAL_FLASH_HEAP_BASE_CASE

// Entry point
extern "C" void app_main(void)
{
    // // Initialise the platform
    // platformInit();

    // Start the main task
    // TODO - decide if WDTs should be enabled
    // TODO - look at esp32-arduino startup code and decide if NVS init there is necessary
    // TODO also note there is a setting enable app rollback support currently unchecked - maybe relates to OTA etc?
    // TODO LWIP_IP_FRAG flag is turned on - check ok
    // TODO LWIP_SO_RECVBUF is turned on - check ok
    // TODO MBEDTLS_HARDWARE_MPI is turned on - check ok
    // TODO - BLE stack runs on core 0 in the menuconfig - check ok
    // TODO - check stack sizes in menuconfig
    // Review stack size

    // Initialize flash
    esp_err_t flashInitResult = nvs_flash_init();
    if (flashInitResult != ESP_OK)
    {
        // Error message
        LOG_W(MODULE_NAME, "nvs_flash_init() failed with error %s (%d)", esp_err_to_name(flashInitResult), flashInitResult);

        // Clear flash if possible
        if ((flashInitResult == ESP_ERR_NVS_NO_FREE_PAGES) || (flashInitResult == ESP_ERR_NVS_NEW_VERSION_FOUND))
        {
            esp_err_t flashEraseResult = nvs_flash_erase();
            if (flashEraseResult != ESP_OK)
            {
                LOG_E(MODULE_NAME, "nvs_flash_erase() failed with error %s (%d)", esp_err_to_name(flashEraseResult), flashEraseResult);
            }
            flashInitResult = nvs_flash_init();
            if (flashInitResult != ESP_OK)
            {
                // Error message
                LOG_W(MODULE_NAME, "nvs_flash_init() failed a second time with error %s (%d)", esp_err_to_name(flashInitResult), flashInitResult);
            }
        }
    }

    // Disable the watchdog timer on the idle task of the core that the mainTask runs on
    // The idle task will not be scheduled to run at all unless: the mainTask blocks, the mainTask delays for at least one tick
    // Since neither of these currently happen it makes more sense just to disable the watchdog as the idle task doesn't do anything in any case
    TaskHandle_t idleTaskOnMainTaskCore = xTaskGetIdleTaskHandleForCPU(MAIN_TASK_PROCESSOR_CORE);
    if (idleTaskOnMainTaskCore)
        esp_task_wdt_delete(idleTaskOnMainTaskCore);

    // TODO - decide if this is necessary - seems to be on new hardware??
    // TaskHandle_t idleTaskOnProCore = xTaskGetIdleTaskHandleForCPU(PRO_TASK_PROCESSOR_CORE);
    // if (idleTaskOnProCore)
    //     esp_task_wdt_delete(idleTaskOnProCore);

    // Start the mainTask
    xTaskCreatePinnedToCore(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, nullptr, MAIN_TASK_PRIORITY, &mainTaskHandle, MAIN_TASK_PROCESSOR_CORE);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test Entry point for Minimal Flash/Heap
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef TEST_MINIMAL_FLASH_HEAP_BASE_CASE
extern "C" void app_main(void)
{
    int i = 0;
    while (1) {
        printf("[%d] Hello world! heap %d\n", i, heap_caps_get_free_size(MALLOC_CAP_8BIT));
        i++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main execution task - started from main
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mainTask(void *pvParameters)
{
    // Trace heap allocation - see RdWebConnManager.cpp and RdWebConnection.h/cpp
#ifdef DEBUG_HEAP_ALLOCATION
    heap_trace_init_standalone(trace_record, NUM_RECORDS);
#endif
    // Config for hardware
    ConfigBase defaultSystemConfig(defaultConfigJSON);

    // Check defaultConfigJSON is valid
    int numJsonTokens = 0;
    if (!RdJson::validateJson(defaultConfigJSON, numJsonTokens))
    {
        LOG_E(MODULE_NAME, "mainTask defaultConfigJSON failed to parse");
    }

    ///////////////////////////////////////////////////////////////////////////////////
    // NOTE: Changing the size or order of the following will affect the layout
    // of data in the Non-Volatile-Storage partition (see paritions.csv file NVS entry)
    // This will render data in those areas invalid and this data will be lost when
    // firmware with different layout is flashed
    ///////////////////////////////////////////////////////////////////////////////////

    // Configuration for the system - including robot name
    ConfigNVS _sysModMutableConfig("system", 500, false);

    // Configuration for PiCoProcessor
    ConfigNVS _piCoProConfig("pi", 2000, false);

    // Configuration for other system modules
    ConfigNVS _sysTypeConfig("sys", 10000, false);

    ///////////////////////////////////////////////////////////////////////////////////

    // SysTypes
    SysTypeManager _sysTypeManager(_sysTypeConfig);
    _sysTypeManager.setup(RIC_SYS_TYPE_STATICS, RIC_SYS_TYPE_STATICS_LEN);

    // Handle power-up LED setting as early as possible
#ifdef FEATURE_POWER_UP_LED_ASAP
    powerUpLEDSet("PowerUpLED", _sysTypeConfig);
#ifdef DEBUG_TIMING_OF_STARTUP
    LOG_I(MODULE_NAME, "powerUpLEDSet ASAP");
#endif
#endif

    // System Module Manager
    SysManager _SysManager("SysManager", defaultSystemConfig, &_sysTypeConfig, &_sysModMutableConfig);

    // API Endpoints
    RestAPIEndpointManager _restAPIEndpointManager;
    _SysManager.setRestAPIEndpoints(_restAPIEndpointManager);

    // ProtocolEndpointManager
    ProtocolEndpointManager _protocolEndpointManager("ProtoMgr", defaultSystemConfig, &_sysTypeConfig, nullptr);
    _SysManager.setProtocolEndpointManager(_protocolEndpointManager);

    // SerialConsole
    SerialConsole _serialConsole("SerialConsole", defaultSystemConfig, &_sysTypeConfig, nullptr);

    // FileManager
    FileManager _fileManager("FileManager", defaultSystemConfig, &_sysTypeConfig, nullptr);

#ifdef FEATURE_WIFI_FUNCTIONALITY
    // NetworkManager
    NetworkManager _networkManager("NetMan", defaultSystemConfig, &_sysTypeConfig, nullptr, DEFAULT_HOSTNAME);
#endif

    // ESP OTA Update
    ESPOTAUpdate _espotaUpdate("ESPOTAUpdate", defaultSystemConfig, &_sysTypeConfig, nullptr);

    // NetDiscovery
    // NetDiscovery _netDiscovery("NetDiscovery", defaultSystemConfig, &_sysTypeConfig, nullptr);

#ifdef FEATURE_WEB_SERVER_OR_WEB_SOCKETS
    // WebServer
    WebServer _webServer("WebServer", defaultSystemConfig, &_sysTypeConfig, nullptr);
#endif

    // Command Serial
    CommandSerial _commandSerial("CommandSerial", defaultSystemConfig, &_sysTypeConfig, nullptr);

    // Command Socket
    CommandSocket _commandSocket("CommandSocket", defaultSystemConfig, &_sysTypeConfig, nullptr);

    //Command File
    CommandFile _commandFile("CommandFile", defaultSystemConfig, &_sysTypeConfig, nullptr);

    // Pi CoProcessor
    PiCoProcessor _piCoPorcessor("PiCoProcessor", defaultSystemConfig, &_sysTypeConfig, &_piCoProConfig, SYSTEM_VERSION);

    // Log out system info
    ESP_LOGI(MODULE_NAME, SYSTEM_NAME " " SYSTEM_VERSION " (built " __DATE__ " " __TIME__ ") Heap %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // SysTypeManager endpoints
    _sysTypeManager.addRestAPIEndpoints(_restAPIEndpointManager);

    // System module manager
    _SysManager.setup();

#ifdef FEATURE_WEB_SERVER_STATIC_FILES
    // Web server add files
    _webServer.serveStaticFiles("/files/spiffs", "/spiffs/");
    _webServer.serveStaticFiles("/files/sd", "/sd/");
    _webServer.serveStaticFiles("/", ("/" + fileSystem.getDefaultFSRoot()).c_str());
#endif

#ifdef FEATURE_WEB_SOCKETS
    // Serve websockets
    _webServer.webSocketSetup("/ws");
#endif

#ifdef FEATURE_WEB_SERVER_OR_WEB_SOCKETS
    // Start webserver
    _webServer.beginServer();
#endif

    // Loop forever
    while (1)
    {
        // Yield for 1 tick
        vTaskDelay(1);

        // Service all the system modules
        _SysManager.service();

        // Test and Monitoring
#ifdef DEBUG_SHOW_TASK_INFO
        if (Utils::isTimeout(millis(), lasttaskdumpms, DEBUG_SHOW_TASK_MS))
        {
            tasks_info();
            lasttaskdumpms = millis();
        }
#endif
#ifdef DEBUG_SHOW_RUNTIME_STATS
        if (Utils::isTimeout(millis(), laststatsdumpms, DEBUG_SHOW_RUNTIME_STATS_MS))
        {
            runtime_stats();
            laststatsdumpms = millis();
        }
#endif

        // TODO - handle XON/XOFF support in serial console or similar - so typing in a command like w/.../...
        // stops logging to serial when the user is entering the command
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#include "freertos/task.h"

#ifdef DEBUG_SHOW_TASK_INFO
int tasks_info()
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = (char*)malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == nullptr) {
        ESP_LOGE("Main", "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}
#endif

#ifdef DEBUG_SHOW_RUNTIME_STATS
int runtime_stats()
{
    const size_t bytes_per_task = 50; /* see vTaskGetRunTimeStats description */
    char *task_stats_buffer = (char*)malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_stats_buffer == nullptr) {
        ESP_LOGE("Main", "failed to allocate buffer for vTaskGetRunTimeStats output");
        return 1;
    }
    fputs("Task Name\tTime(uS)\t%", stdout);
    fputs("\n", stdout);
    vTaskGetRunTimeStats(task_stats_buffer);
    fputs(task_stats_buffer, stdout);
    free(task_stats_buffer);
    return 0;
}
#endif
#endif
