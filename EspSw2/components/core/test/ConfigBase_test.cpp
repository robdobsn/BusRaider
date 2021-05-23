/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit tests of ConfigBase
//
// Rob Dobson 2017-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <Logger.h>
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <ConfigBase.h>
#include <ESPUtils.h>
#include <WString.h>
#include "unity.h"

static const char* MODULE_PREFIX = "ConfBaseUnitTest";

static bool testGetString(const char* dataPath, const char* expectedStr, const char* pSourceStr)
{
    // Create ConfigBase
    ConfigBase testConfigBase = pSourceStr;

    // Get string
    String rsltStr = testConfigBase.getString(dataPath, "<<NOT_FOUND>>");
    // LOG_I(MODULE_PREFIX, "testGetString dataPath %s expected %s", dataPath, expectedStr);
    if (!rsltStr.equals(expectedStr))
    {
        LOG_E(MODULE_PREFIX, "testGetString dataPath %s expected %s != %s", dataPath, expectedStr, rsltStr.c_str());
        return false;
    }
    return true;
}

// Compare the results from ConfigBase::getString() with RdJson::getString()
static bool testConfigVsJson(const char* dataPath, const char* pSourceStr)
{
    // Create ConfigBase
    ConfigBase testConfigBase = pSourceStr;

    // Get string
    String cfgRslt = testConfigBase.getString(dataPath, "<<NOT_FOUND>>");
    String jsonRslt = RdJson::getString(dataPath, "<<NOT_FOUND>>", pSourceStr);
    // LOG_I(MODULE_PREFIX, "testGetString dataPath %s expected %s", dataPath, expectedStr);
    if (!cfgRslt.equals(jsonRslt))
    {
        LOG_E(MODULE_PREFIX, "testConfigVsJson dataPath %s expected %s != %s",
                dataPath, jsonRslt.c_str(), cfgRslt.c_str());
        return false;
    }

    // Get long
    long cfgLong = testConfigBase.getLong(dataPath, -1);
    long jsonLong = RdJson::getLong(dataPath, -1, pSourceStr);
    // LOG_I(MODULE_PREFIX, "testGetString dataPath %s expected %s", dataPath, expectedStr);
    if (jsonLong != cfgLong)
    {
        LOG_E(MODULE_PREFIX, "testConfigVsJson dataPath %s expected %ld != %ld",
                dataPath, jsonLong, cfgLong);
        return false;
    }

    // Get long
    double cfgDouble = testConfigBase.getDouble(dataPath, -1.5);
    double jsonDouble = RdJson::getDouble(dataPath, -1.5, pSourceStr);
    // LOG_I(MODULE_PREFIX, "testGetString dataPath %s expected %s", dataPath, expectedStr);
    if (jsonDouble != cfgDouble)
    {
        LOG_E(MODULE_PREFIX, "testConfigVsJson dataPath %s expected %f != %f",
                dataPath, jsonDouble, cfgDouble);
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
    R"("SysManager":{"monitorPeriodMs":10000,"reportList":["NetMan","RobotCtrl"]},)"
    R"("NetMan":{"WiFiEnabled":1, "defaultHostname":"Marty", "logLevel":"D"},)"
    R"("NTPClient":{"enable":1,"NTPServer":"pool.ntp.org", "GMTOffsetSecs":0, "DSTOffsetSecs":0},)"
    R"("MQTTManager":{"enable":0},)"
    R"("ESPOTAUpdate":{"enable":1,"OTADirectEnabled":0,"server":"192.168.86.235","port":5076,)"
            R"("sysName":")" SYSTEM_NAME R"(","sysVers":")" SYSTEM_VERSION R"("},)"
    R"("FileManager":{"SPIFFSEnabled":1,"SPIFFSFormatIfCorrupt":1,"SDEnabled":0,"CacheFileList":0},)"
    R"("WebServer":{"enable":1,"webServerPort":80,"allowOriginAll":1,"apiPrefix":"api/","logLevel":"D"},)"
    R"("SerialConsole":{"enable":1,"uartNum":0,"baudRate":115200,"crlfOnTx":1,"logLevel":"D"},)"
    R"("CommandSerial":{"enable":1,"uartNum":1,"baudRate":912600,"rxBufSize":1024,"rxPin":35,"txPin":12,)"
            R"("protocol":"RICSerial","logLevel":"D"},)"
    R"("TelnetServer":{"enable":1,"port":23},)"
    R"("CommandSocket":{"enable":1,"socketPort":24,"protocol":"Marty1ShortCodes","logLevel":"D"})"
    R"(})"
    ;

const int hwRev = getHwRevision();
const String curHwRev(hwRev);
const String otherHwRev(curHwRev + 100);
const String revisionSwitchedJson(
    R"({)"
    R"(    "StringSetting": [)"
    R"(        {"__hwRevs__": [], "__value__": "defaultVal"},)"
    R"(        {"__hwRevs__": [ )" + otherHwRev + R"(], "__value__": "wrongVal"},)"
    R"(        {"__hwRevs__": [ )" + curHwRev + R"(, 1000], "__value__": "correctVal"})"
    R"(    ],)"
    R"(    "StringDefault": [)"
    R"(        {"__hwRevs__": [1000, 1001, 1002], "__value__": "wrongVal"},)"
    R"(        {"__hwRevs__": [], "__value__": "defaultVal"},)"
    R"(        {"__hwRevs__": [1003, 1004], "__value__": "alsoWrongVal"})"
    R"(    ],)"
    R"(    "pi": [)"
    R"(        {"__hwRevs__": [1000,  )" + curHwRev + R"(], "__value__": 3.1415},)"
    R"(        {"__hwRevs__": [], "__value__": 3})"
    R"(    ],)"
    R"(    "e": [)"
    R"(        {"__hwRevs__": [], "__value__": 3},)"
    R"(        {"__hwRevs__": [1000,  )" + curHwRev + R"(, 1001], "__value__": 2.718})"
    R"(    ],)"
    R"(    "IntSetting": [)"
    R"(        {"__hwRevs__": [], "__value__": 42},)"
    R"(        {"__hwRevs__": [1000], "__value__": -1})"
    R"(    ],)"
    R"(    "NoValue": [)"
    R"(        {"__hwRevs__": [1000], "__value__": 0},)"
    R"(        {"__hwRevs__": [1001], "__value__": 1})"
    R"(    ],)"
    R"(    "ArraySetting": [)"
    R"(        {"__hwRevs__": [], "__value__": [10, 20, 30]})"
    R"(    ],)"
    R"(    "ObjectSetting": [)"
    R"(        {"__hwRevs__": [ )" + curHwRev + R"(], "__value__": {"str": "hello", "int": 42, "float": -0.1}})"
    R"(    ],)"
    R"(    "ArrayWithSwitchedElem": [)"
    R"(        "normalElement1",)"
    R"(        "normalElement2",)"
    R"(        [)"
    R"(            {"__hwRevs__": [], "__value__": "defaultSwitchedElem"},)"
    R"(            {"__hwRevs__": [ )" + otherHwRev + R"(], "__value__": "wrongSwitchedElem"},)"
    R"(            {"__hwRevs__": [ )" + curHwRev + R"(], "__value__": "correctSwitchedElem"})"
    R"(        ],)"
    R"(        "normalElement3")"
    R"(    ])"
    R"(})"
);
String switchedElem(
    R"([)"
    R"(            {"__hwRevs__": [], "__value__": "defaultSwitchedElem"},)"
    R"(            {"__hwRevs__": [ )" + otherHwRev + R"(], "__value__": "wrongSwitchedElem"},)"
    R"(            {"__hwRevs__": [ )" + curHwRev + R"(], "__value__": "correctSwitchedElem"})"
    R"(        ])"
);

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
        { "SysManager/reportList", R"(["NetMan","RobotCtrl"])" },
        { "SysManager/reportList[1]", "RobotCtrl" },
        { "NetMan", R"({"WiFiEnabled":1,"defaultHostname":"Marty","logLevel":"D"})" },
        { "WebServer/apiPrefix", "api/" },
        { "CommandSocket/enable", "1" },
    };
    for (int testIdx = 0; testIdx < sizeof(getStringTests)/sizeof(getStringTests[0]); testIdx++)
    {
        String tokStartStr = "testGetString testIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(testGetString(getStringTests[testIdx].dataPath,
                                          getStringTests[testIdx].expStr,
                                          testJSONConfigBase), tokStartStr.c_str());
        TEST_ASSERT_MESSAGE(testConfigVsJson(getStringTests[testIdx].dataPath,
                                             testJSONConfigBase), tokStartStr.c_str());
    }

    // Debug
    // LOG_I(MODULE_PREFIX, "Revision switched JSON string:\n%s", revisionSwitchedJson.c_str());

    TestElem revisionSwitchTests [] = {
        { "StringSetting", "correctVal" },
        { "StringDefault", "defaultVal" },
        { "pi", "3.1415" },
        { "IntSetting", "42" },
        { "ArraySetting", "[10,20,30]" },
        { "ObjectSetting", R"({"str":"hello","int":42,"float":-0.1})" },
        { "ArrayWithSwitchedElem[0]", "normalElement1" },
        { "ArrayWithSwitchedElem[2]", "correctSwitchedElem" },
        { "ArrayWithSwitchedElem/[3]", "normalElement3" },
        { "NoValue", "<<NOT_FOUND>>" },
        { "NONEXISTENT_KEY", "<<NOT_FOUND>>" },
        { "ObjectSetting/str", "<<NOT_FOUND>>" },  //TODO: Ideally, we'd make this retrieve "hello" in the future
    };
    int numRevisionSwitchTests = sizeof(revisionSwitchTests)/sizeof(revisionSwitchTests[0]);
    for (int testIdx = 0; testIdx < numRevisionSwitchTests; testIdx++)
    {
        String tokStartStr = "testGetString testIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(testGetString(revisionSwitchTests[testIdx].dataPath,
                                          revisionSwitchTests[testIdx].expStr,
                                          revisionSwitchedJson.c_str()), tokStartStr.c_str());
    }
}

TEST_CASE("test_getLong", "[ConfigBase]")
{
    ConfigBase config(revisionSwitchedJson);

    long result = config.getLong("IntSetting", -2);
    TEST_ASSERT_EQUAL_INT(42, result);

    result = config.getLong("ArraySetting/[0]/__value__[1]", -3);
    TEST_ASSERT_EQUAL_INT(20, result);

    result = config.getLong("NoValue", -4);
    TEST_ASSERT_EQUAL_INT(-4, result);
}

TEST_CASE("test_getDouble", "[ConfigBase]")
{
    ConfigBase config(revisionSwitchedJson);

    double result = config.getDouble("e", -0.5);
    TEST_ASSERT_EQUAL_DOUBLE(2.718, result);

    result = config.getDouble("pi/[0]/__value__", -0.25);
    TEST_ASSERT_EQUAL_DOUBLE(3.1415, result);

    result = config.getDouble("NoValue", -0.125);
    TEST_ASSERT_EQUAL_DOUBLE(-0.125, result);
}

TEST_CASE("test_getArrayElems", "[ConfigBase]")
{
    ConfigBase config(revisionSwitchedJson);
    std::vector<String> result;
    bool success;

    {
        success = config.getArrayElems("ArraySetting", result);
        TEST_ASSERT_TRUE_MESSAGE(success, "Could not get array elems of 'ArraySetting'");
        const char* resultArr[result.size()];
        for (int i = 0; i < result.size(); ++i)
            resultArr[i] = result[i].c_str();
        const char* expectedArr[3] = {"10", "20", "30"};
        TEST_ASSERT_EQUAL_STRING_ARRAY(expectedArr, resultArr, result.size());
    }
    {
        success = config.getArrayElems("ArrayWithSwitchedElem", result);
        TEST_ASSERT_TRUE_MESSAGE(success, "Could not get array elems of 'ArrayWithSwitchedElem'");

        const char* resultArr[result.size()];
        for (int i = 0; i < result.size(); ++i)
            resultArr[i] = result[i].c_str();

        const char* expectedArr[4] = {"normalElement1", "normalElement2", "", "normalElement3"};
        expectedArr[2] = switchedElem.c_str();

        TEST_ASSERT_EQUAL_STRING_ARRAY(expectedArr, resultArr, result.size());
    }
    success = config.getArrayElems("StringSetting", result);
    TEST_ASSERT_FALSE_MESSAGE(success, "'StringSetting' misinterpreted as array");
}

TEST_CASE("test_getKeys", "[ConfigBase]")
{
    ConfigBase config(revisionSwitchedJson);
    std::vector<String> result;
    bool success;

    {
        success = config.getKeys("ObjectSetting", result);
        TEST_ASSERT_TRUE_MESSAGE(success, "Could not get keys of 'ObjectSetting'");
        const char* resultArr[result.size()];
        for (int i = 0; i < result.size(); ++i)
            resultArr[i] = result[i].c_str();
        const char* expectedArr[3] = {"str", "int", "float"};
        TEST_ASSERT_EQUAL_STRING_ARRAY(expectedArr, resultArr, result.size());
    }
    {
        success = config.getKeys("", result);
        TEST_ASSERT_TRUE_MESSAGE(success, "Could not get keys of ''");
        const char* resultArr[result.size()];
        for (int i = 0; i < result.size(); ++i)
            resultArr[i] = result[i].c_str();
        const char* expectedArr[9] = {"StringSetting", "StringDefault", "pi", "e", "IntSetting",
                                      "NoValue", "ArraySetting", "ObjectSetting", "ArrayWithSwitchedElem"};
        TEST_ASSERT_EQUAL_STRING_ARRAY(expectedArr, resultArr, result.size());
    }
    {
        success = config.getKeys("pi[0]", result);
        TEST_ASSERT_TRUE_MESSAGE(success, "Could not get keys of 'pi[0]'");
        const char* resultArr[result.size()];
        for (int i = 0; i < result.size(); ++i)
            resultArr[i] = result[i].c_str();
        const char* expectedArr[2] = {"__hwRevs__", "__value__"};
        TEST_ASSERT_EQUAL_STRING_ARRAY(expectedArr, resultArr, result.size());
    }

    // These should fail
    success = config.getKeys("StringSetting", result);
    TEST_ASSERT_FALSE_MESSAGE(success, "'StringSetting' misinterpreted as object");
    success = config.getKeys("NoValue", result);
    TEST_ASSERT_FALSE_MESSAGE(success, "'NoValue' misinterpreted as object");
    success = config.getKeys("NONEXISTENT_KEY", result);
    TEST_ASSERT_FALSE_MESSAGE(success, "'NONEXISTENT_KEY' misinterpreted as object");
}