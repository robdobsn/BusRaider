/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit tests of ConfigBase
//
// Rob Dobson 2017-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ConfigBase.h>
#include <Logger.h>
#include "unity.h"

static const char* MODULE_PREFIX = "ConfBaseUnitTest";

static bool testGetString(const char* dataPath, const char* expectedStr, const char* pSourceStr)
{
    // Create ConfigBase
    ConfigBase testConfigBase = pSourceStr;

    // Get string
    String rsltStr = testConfigBase.getString(dataPath, "<<TEST_FAILED>>");
    // LOG_I(MODULE_PREFIX, "testGetString dataPath %s expected %s", dataPath, expectedStr);
    if (!rsltStr.equals(expectedStr))
    {
        LOG_I(MODULE_PREFIX, "testGetString dataPath %s expected %s != %s", dataPath, expectedStr, rsltStr.c_str());
        return false;
    }
    return true;
}

#define SYSTEM_NAME "RicFirmwareESP32"
#define SYSTEM_VERSION "0.0.3"
const char* testJSONConfigBase = 
    R"({)"
    R"("SystemName":")" SYSTEM_NAME R"(",)"
    R"("SystemVersion":")" SYSTEM_VERSION R"(",)"
    R"("IDFVersion":")" IDF_VER R"(",)"
    R"("SysManager":{"monitorPeriodMs":10000,"reportList":["NetworkManager","RobotController"]},)"
    R"("DefaultSysType":"RICMarty2",)"
    R"("NetworkManager":{"WiFiEnabled":1,"defaultHostname":"Marty","logLevel":"D"},)"
    R"("NTPClient":{"enable":1,"NTPServer":"pool.ntp.org", "GMTOffsetSecs":0, "DSTOffsetSecs":0},)"
    R"("MQTTManager":{"enable":0},)"
    R"("ESPOTAUpdate":{"enable":1,"OTADirectEnabled":0,"server":"192.168.86.235","port":5076,)"
            R"("sysName":")" SYSTEM_NAME R"(","sysVers":")" SYSTEM_VERSION R"("},)"
    R"("FileManager":{"SPIFFSEnabled":1,"SPIFFSFormatIfCorrupt":1,"SDEnabled":0,"CacheFileList":0},)"
    R"("WebServer":{"enable":1,"webServerPort":80,"allowOriginAll":1,"apiPrefix":"api/","logLevel":"D"},)"
    R"("SerialConsole":{"enable":1,"uartNum":0,"baudRate":115200,"crlfOnTx":1,"logLevel":"D"},)"
    R"("CommandSerial":{"enable":1,"uartNum":1,"baudRate":912600,"rxBufSize":1024,"rxPin":35,"txPin":12,)"
            R"("hdlcRxMaxLen":1024,"protocol":"RICSerial","logLevel":"D"},)"
    R"("TelnetServer":{"enable":1,"port":23},)"
    R"("CommandSocket":{"enable":1,"socketPort":24,"protocol":"Marty1ShortCodes","logLevel":"D"})"
    R"(})"
    ;


TEST_CASE("test_getString", "[ConfigBase]")
{
    // Debug
    // LOG_I(MODULE_PREFIX, "JSON input string\n%s", testJSONConfigBase);

    // Test getString
    struct TestElem
    {
        const char* dataPath;
        const char* expStr;
    };
    TestElem getStringTests [] = {
        { "SystemName", "RicFirmwareESP32" },
        { "SysManager/reportList[1]", "RobotController" },
        { "BLEManager", R"({"enable":1,"adName":"Marty","logLevel":"D"})" },
        { "BLEManager/adName", "Marty" },
        { "CommandSocket/enable", "1" },
    };
    for (int testIdx = 0; testIdx < sizeof(getStringTests)/sizeof(getStringTests[0]); testIdx++)
    {
        String tokStartStr = "testGetString testIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(true == testGetString(getStringTests[testIdx].dataPath, 
                    getStringTests[testIdx].expStr, testJSONConfigBase), tokStartStr.c_str());
    }
}