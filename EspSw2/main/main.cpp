/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Main entry point
//
// BusRaider ESP32
// Rob Dobson 2018-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Name and Version
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define SYSTEM_NAME "BusRaiderESP32"
#define SYSTEM_VERSION "3.1.0"
#define DEFAULT_SYSTYPE "BusRaider"
#define DEFAULT_SYSNAME "BusRaider"
#define DEFAULT_HOSTNAME "BusRaider"

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
        R"("DefaultSysType":")" DEFAULT_SYSTYPE R"(",)"
        R"("DefaultName":")" DEFAULT_SYSNAME R"(",)"
        R"("DefaultNameIsSet":0,)"
        R"("SysManager":{)"
            R"("monitorPeriodMs":10000,)"
            R"("reportList":["NetworkManager"],)"
            R"("pauseWiFiforBLE":1)"
        R"(},)"
        R"("NetworkManager":{)"
            R"("WiFiEnabled":1,)"
            R"("defaultHostname":")" DEFAULT_HOSTNAME R"(",)"
            R"("logLevel":"D")"
        R"(},)"
        R"("NTPClient":{)"
            R"("enable":1,)"
            R"("NTPServer":"pool.ntp.org",)"
            R"("GMTOffsetSecs":0,)"
            R"("DSTOffsetSecs":0)"
        R"(},)"
        R"("ESPOTAUpdate":{)"
            R"("OTADirect":1)"
        R"(},)"
        R"("FileManager":{)"
            R"("SPIFFSEnabled":1,)"
            R"("SPIFFSFormatIfCorrupt":1,)"
            R"("SDEnabled":0,)"
            R"("CacheFileList":0)"
        R"(},)"
        R"("WebServer":{)"
            R"("enable":1,)"
            R"("webServerPort":80,)"
            R"("allowOriginAll":1,)"
            R"("apiPrefix":"api/",)"
            R"("numConnSlots":6,)"
            R"("maxWebSockets":3,)"
            R"("pingIntervalMs":2000,)"
            R"("logLevel":"D")"
        R"(},)"
        R"("SerialConsole":{)"
            R"("enable":1,)"
            R"("uartNum":0,)"
            R"("baudRate":115200,)"
            R"("crlfOnTx":1,)"
            R"("protocol":"RICSerial",)"
            R"("logLevel":"D")"
        R"(},)"
        R"("CommandSerial":{)"
            R"("enable":0,)"
            R"("uartNum":1,)"
            R"("baudRate":912600,)"
            R"("rxBufSize":32768,)"
            R"("rxPin":16,)"
            R"("txPin":17,)"
            R"("hdlcRxMaxLen":5000,)"
            R"("protocol":"RICSerial",)"
            R"("logLevel":"D")"
        R"(},)"
        R"("CommandSocket":{)"
            R"("enable":1,)"
            R"("socketPort":24,)"
            R"("protocol":"BusRaiderCSOK",)"
            R"("logLevel":"D")"
        R"(},)"
        R"("PiCoProcessor":{)"
            R"("enable":1,)"
            R"("uartNum":2,)"
            R"("baudRate":912600,)"
            R"("rxBufSize":32768,)"
            R"("rxPin":16,)"
            R"("txPin":17,)"
            R"("hdlcMaxLen":5000,)"
            R"("protocol":"RICSerial",)"
            R"("logLevel":"D")"
        R"(})"
    R"(})"
;

const char* SYS_CONFIGURATIONS[] = {
    R"({)"
        R"("SysType":"BusRaider")"
    R"(})"
};    
extern const int SYS_CONFIGURATIONS_LEN = sizeof(SYS_CONFIGURATIONS) / sizeof(const char *);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// System Parameters
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Main task parameters
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const int MAIN_TASK_PRIORITY = 20;
static const int MAIN_TASK_PROCESSOR_CORE = 1;
static const int MAIN_TASK_STACK_SIZE = 16384;
static TaskHandle_t mainTaskHandle = NULL;
extern void mainTask(void *pvParameters);

// Include WiFi functionality
#define INCLUDE_WIFI_FUNCTIONALITY

// Debug
static const char* MODULE_NAME = "MainTask";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test and Monitoring
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #define TEST_MINIMAL_FLASH_HEAP_BASE_CASE 1
// #define DEBUG_SHOW_TASK_INFO 1
// #define DEBUG_SHOW_RUNTIME_STATS 1

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
#ifdef INCLUDE_WIFI_FUNCTIONALITY
#include <NetworkManager.h>
#include <NetDiscovery.h>
#include <WebServer.h>
#endif
#include <CommandSerial.h>
#include <CommandSocket.h>
#include <CommandFile.h>
#include <FileSystem.h>
#include <ESPOTAUpdate.h>
#include <PiCoProcessor.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Standard Entry Point
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef TEST_MINIMAL_FLASH_HEAP_BASE_CASE

// Entry point
extern "C" void app_main(void)
{
    // Initialize flash
    nvs_flash_init();

    // Disable the watchdog timer on the idle task of the core that the mainTask runs on
    // The idle task will not be scheduled to run at all unless: the mainTask blocks, the mainTask delays for at least one tick
    // Since neither of these currently happen it makes more sense just to disable the watchdog as the idle task doesn't do anything in any case
    TaskHandle_t idleTaskOnMainTaskCore = xTaskGetIdleTaskHandleForCPU(MAIN_TASK_PROCESSOR_CORE);
    if (idleTaskOnMainTaskCore)
        esp_task_wdt_delete(idleTaskOnMainTaskCore);

    // Start the mainTask
    xTaskCreatePinnedToCore(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, &mainTaskHandle, MAIN_TASK_PROCESSOR_CORE);
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
    // Config for hardware
    ConfigBase defaultSystemConfig(defaultConfigJSON);

    // Check defaultConfigJSON is valid
    int numJsonTokens = 0;
    if (!defaultSystemConfig.validateJson(numJsonTokens))
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
    ConfigNVS _sysModMutableConfig("system", 500);

    // Configuration for other system modules
    ConfigNVS _sysTypeConfig("sys", 10000);

    ///////////////////////////////////////////////////////////////////////////////////

    // SysTypes
    SysTypeManager _sysTypeManager;

    // System Module Manager
    SysManager _SysManager("SysManager", defaultSystemConfig, &_sysModMutableConfig);

    // API Endpoints
    RestAPIEndpointManager _restAPIEndpointManager;
    _SysManager.setRestAPIEndpoints(_restAPIEndpointManager);

    // ProtocolEndpointManager
    ProtocolEndpointManager _protocolEndpointManager("ProtocolEndpointManager", defaultSystemConfig, &_sysTypeConfig, NULL);
    _SysManager.setProtocolEndpointManager(_protocolEndpointManager);

    // SerialConsole
    SerialConsole _serialConsole("SerialConsole", defaultSystemConfig, &_sysTypeConfig, NULL);

    // FileManager
    FileManager _fileManager("FileManager", defaultSystemConfig, &_sysTypeConfig, NULL);

#ifdef INCLUDE_WIFI_FUNCTIONALITY
    // NetworkManager
    NetworkManager _networkManager("NetworkManager", defaultSystemConfig, &_sysTypeConfig, NULL);
#endif

    // ESP OTA Update
    ESPOTAUpdate _espotaUpdate("ESPOTAUpdate", defaultSystemConfig, &_sysTypeConfig, NULL);

#ifdef INCLUDE_WIFI_FUNCTIONALITY
    // WebServer
    WebServer _webServer("WebServer", defaultSystemConfig, &_sysTypeConfig, NULL);
#endif

    // Command Serial
    CommandSerial _commandSerial("CommandSerial", defaultSystemConfig, &_sysTypeConfig, NULL);

    // Command Socket
    CommandSocket _commandSocket("CommandSocket", defaultSystemConfig, &_sysTypeConfig, NULL);

    //Command File
    CommandFile _commandFile("CommandFile", defaultSystemConfig, &_sysTypeConfig, NULL);

    // Pi CoProcessor
    PiCoProcessor _piCoPorcessor("PiCoProcessor", defaultSystemConfig, &_sysTypeConfig, NULL);

    // Log out system info
    ESP_LOGI(MODULE_NAME, SYSTEM_NAME " " SYSTEM_VERSION " (built " __DATE__ " " __TIME__ ") Heap %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // SysTypeManager setup
    _sysTypeManager.setup(SYS_CONFIGURATIONS, SYS_CONFIGURATIONS_LEN, &_sysTypeConfig);
    _sysTypeManager.addRestAPIEndpoints(_restAPIEndpointManager);

    // System module manager
    _SysManager.setup();

#ifdef INCLUDE_WIFI_FUNCTIONALITY
    // Web server add files
    _webServer.serveStaticFiles("/files/spiffs", "/spiffs/");
    _webServer.serveStaticFiles("/files/sd", "/sd/");
    _webServer.serveStaticFiles("/", ("/" + fileSystem.getDefaultFSRoot()).c_str());

    // Open websocket
    _webServer.webSocketOpen("/ws");

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
    if (task_list_buffer == NULL) {
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
    if (task_stats_buffer == NULL) {
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
