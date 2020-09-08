/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit test of RDJSON Performance
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <WString.h>
#include <RdJson.h>
#include <Logger.h>
#include "unity.h"
#include <jsmnR.h>
#include <Utils.h>
#include <ConfigBase.h>

static const char* MODULE_PREFIX = "RdJsonPerfTest";

static uint64_t perfStartTimeUs;
static uint64_t perfParseUs;
static uint64_t perfFindElemEndUs;
static uint64_t perfFindKeyUs;
static uint64_t perfGetString1Us;
static uint64_t perfLoop100Us;
#define EVAL_PERF_CLEAR(SVar) SVar = 0
#define EVAL_PERF_START() perfStartTimeUs = micros();
#define EVAL_PERF_ACCUM(SVar) {SVar += (micros() - perfStartTimeUs);}
#define EVAL_PERF_END(SVar) {SVar = (micros() - perfStartTimeUs);}

static bool testFindElemEnd(jsmntok_t* pTokens, int numTokens, int tokenIdx, int expEndPos, const char* pSourceStr)
{
    
    // Find element end
    int endPos = RdJson::findElemEnd(pSourceStr, pTokens, numTokens, tokenIdx);
    // LOG_I(MODULE_PREFIX, "testFindElemEnd startTok %d endTok %d", tokenIdx, endPos);
    if (expEndPos != endPos)
    {
        LOG_I(MODULE_PREFIX, "testFindElemEnd endPos expected %d != %d", expEndPos, endPos);
        return false;
    }
    return true;
}

static bool testFindKeyInJson(jsmntok_t* pTokens, int numTokens, 
                const char* dataPath, const char* expStr, const char* pSourceStr)
{
    // find key
    int endTokenIdx = 0;
    jsmntype_t keyType = JSMN_UNDEFINED;
    int foundTokenIdx = RdJson::findKeyInJson(pSourceStr, pTokens, numTokens, dataPath, endTokenIdx, keyType);
    String elemStr;
    if (foundTokenIdx >= 0)
        Utils::strFromBuffer((uint8_t*)pSourceStr + pTokens[foundTokenIdx].start, 
                pTokens[foundTokenIdx].end - pTokens[foundTokenIdx].start,
                // ((endTokenIdx < numTokens) && (endTokenIdx >= 0)) ? pTokens[foundTokenIdx].end - pTokens[foundTokenIdx].start : strlen(pSourceStr) - pTokens[foundTokenIdx].start,
                elemStr);
    // LOG_I(MODULE_PREFIX, "testFindKeyInJson dataPath %s elemStr %s startTok %d endTok %d", dataPath, elemStr.c_str(), 
    //                         foundTokenIdx, endTokenIdx);
    if (!elemStr.equals(expStr))
    {
        LOG_I(MODULE_PREFIX, "testFindKeyInJson failed expected %s != %s", expStr, elemStr.c_str());
        return false;
    }
    return true;
}

// static bool testGetArrayElems(const char* dataPath, const char* expStrs[], int numStrs, const char* pSourceStr)
// {
//     std::vector<String> arrayElems;
//     bool isValid = RdJson::getArrayElems(dataPath, arrayElems, pSourceStr);
//     // LOG_I(MODULE_PREFIX, "testGetArrayElems dataPath %s got len %d", dataPath, arrayElems.size());
//     if (!isValid)
//     {
//         LOG_I(MODULE_PREFIX, "testGetArrayElems failed");
//         return false;
//     }
//     if (numStrs != arrayElems.size())
//     {
//         LOG_I(MODULE_PREFIX, "testGetArrayElems failed expected len %d != %d", numStrs, arrayElems.size());
//         return false;
//     }
//     for (int i = 0; i < numStrs; i++)
//     {
//         if (!arrayElems[i].equals(expStrs[i]))
//         {
//             LOG_I(MODULE_PREFIX, "testGetArrayElems failed idx %d expected %s != %s", i, expStrs[i], arrayElems[i].c_str());
//             return false;
//         }
//     }
//     return true;
// }

// static bool testGetObjectKeys(const char* dataPath, const char* expStrs[], int numStrs, const char* pSourceStr)
// {
//     std::vector<String> arrayElems;
//     bool isValid = RdJson::getKeys(dataPath, arrayElems, pSourceStr);
//     // LOG_I(MODULE_PREFIX, "testGetObjectKeys dataPath %s got len %d", dataPath, arrayElems.size());
//     if (!isValid)
//     {
//         LOG_I(MODULE_PREFIX, "testGetObjectKeys failed");
//         return false;
//     }
//     if (numStrs != arrayElems.size())
//     {
//         LOG_I(MODULE_PREFIX, "testGetObjectKeys failed expected len %d != %d", numStrs, arrayElems.size());
//         return false;
//     }
//     for (int i = 0; i < numStrs; i++)
//     {
//         if (!arrayElems[i].equals(expStrs[i]))
//         {
//             LOG_I(MODULE_PREFIX, "testGetObjectKeys failed idx %d expected %s != %s", i, expStrs[i], arrayElems[i].c_str());
//             return false;
//         }
//     }
//     return true;
// }

String testVarName;
double testVal;
void testAddVar(const char* varName, double val)
{
    testVarName = varName;
    testVal = val;
}

String testJSONHWElem = 
R"("hw":[{"name":"LeftTwist","type":"SmartServo","busName":"I2CA","addr":"0x11","idx":1,
"whoAmI":"","serialNo":"4f7aa220974cadc7","versionStr":"0.0.0","commsOk":1,
"pos":107.40,"curr":0,"state":0,"velo":-26804}])";

void testAddHwElemInfos()
{
    EVAL_PERF_START();

    String elemName = "testElem";
    // Setup the variables
    for (int i = 0; i < 100; i++)
    {
        // Extract important info from each element and add to variables
        ConfigBase elemConf = testJSONHWElem;
        ConfigBase elemHw = elemConf.getString("hw[0]", "{}");

        // Debug
#ifdef DEBUG_TRAJECTORY_MANAGER
        LOG_I(MODULE_PREFIX, "exec add elemName %s elemHw %s", elemName.c_str(), elemHw.c_str());
#endif

        // Index
        int elemIdx = elemHw.getLong("idx", -1);
        if (elemIdx >= 0)
        {
            // Add variable
            String idxVarName = elemName;
            // _evaluator.addVariable(elemName.c_str(), elemIdx);
            testAddVar(elemName.c_str(), elemIdx);
        }

        // Hardware specific values
        String elemType = elemHw.getString("type", "");
        if (elemType == "SmartServo")
        {
            double pos = elemHw.getDouble("pos", -1e10);
            if (pos > -360)
            {
                String posVarName = elemName + "Pos";
                // _evaluator.addVariable(posVarName.c_str(), pos);
                testAddVar(elemName.c_str(), pos);
            }
        }
    }

    EVAL_PERF_END(perfLoop100Us);

}

TEST_CASE("test_rdjson_perf", "[rdjsonperf]")
{
    const char* testJSON = 
            R"({                                        )"
            R"( "consts": {                             )"
            R"( 	"axis": "1",                        )"
            R"( 	"oxis": {                           )"
            R"( 		"coo": ["pig", 4, "dog", {      )"
            R"( 			"minotaur": [1, 3, 4],      )"
            R"( 			"combine": "aaargh",        )"
            R"( 			"slippery": {               )"
            R"( 				"animal": "goat",       )"
            R"( 				"nice": {},             )"
            R"( 				"polish": "shoes"       )"
            R"( 			},                          )"
            R"( 			"foo": "bar"                )"
            R"( 		}]                              )"
            R"( 	},                                  )"
            R"( 	"exis": "banana",                   )"
            R"( 	"comarr": [6, 5, 4, 3, 3,           )"
            R"( 		{"fish": "stew"}                )"
            R"( 	],                                  )"
            R"( 	"lastly": "elephant"                )"
            R"( }                                       )"
            R"(}                                        )";

    // Parse json into tokens
    int numTokens = 0;

    EVAL_PERF_START();
    jsmntok_t *pTokens = RdJson::parseJson(testJSON, numTokens);
    EVAL_PERF_END(perfParseUs);

    if (pTokens == NULL)
    {
        LOG_I(MODULE_PREFIX, "testFindElemEnd parseJson failed");
        return;
    }

    // Debug
    // RdJson::debugDumpParseResult(testJSON, pTokens, numTokens);

    // Test the findElemEnd function
    int endTokens[] = {44,2,44,4,5,
                    6,30,8,30,10,
                    11,12,30,14,18,
                    16,17,18,19,20,
                    21,28,23,24,25,
                    26,27,28,29,30,
                    31,32,33,42,35,
                    36,37,38,39,42,
                    41,42,43,44};
    EVAL_PERF_CLEAR(perfFindElemEndUs);
    for (int tokStart = 0; tokStart < sizeof(endTokens)/sizeof(int); tokStart++)
    {
        String tokStartStr = "testFindElem start=" + String(tokStart);
        EVAL_PERF_START();
        testFindElemEnd(pTokens, numTokens, tokStart, endTokens[tokStart], testJSON);
        EVAL_PERF_ACCUM(perfFindElemEndUs);
    }

    // Test the findKeyInJson function
    struct TestElem
    {
        const char* dataPath;
        const char* expStr;
    };
    TestElem findKeyTests [] = {
        { "consts/axis", "1" },
        { "consts/oxis/coo[2]", "dog" },
        { "consts/oxis/coo[3]/minotaur", "[1, 3, 4]" },
        { "consts/oxis/coo[3]/combine", "aaargh" },
        { "consts/oxis/coo[3]/slippery/nice", "{}" },
        { "consts/oxis/coo[3]/foo", "bar" },
        { "consts/exis", "banana" },
        { "consts/comarr/[0]", "6" },
        { "consts/comarr/[4]", "3" },
        { "consts/comarr/[5]/fish", "stew" },
        { "consts/lastly", "elephant" },
    };
    EVAL_PERF_CLEAR(perfFindKeyUs);
    for (int testIdx = 0; testIdx < sizeof(findKeyTests)/sizeof(findKeyTests[0]); testIdx++)
    {
        String tokStartStr = "testFindElem testkeyIdx=" + String(testIdx);
        EVAL_PERF_START();
        testFindKeyInJson(pTokens, numTokens, findKeyTests[testIdx].dataPath, findKeyTests[testIdx].expStr, testJSON);
        EVAL_PERF_ACCUM(perfFindKeyUs);
    }

    // Cleanup
    delete[] pTokens;

    String testJsonHw = R"({"name":"LeftTwist","type":"SmartServo","busName":"I2CA","addr":"0x11","idx":"1","whoAmI":"","serialNo":"4f7aa220974cadc7","versionStr":"0.0.0","commsOk":1,"pos":107.70,"curr":0,"state":0,"velo":-26804})";
    // // Test higher level methods
    EVAL_PERF_START();
    for (int i = 0; i < 100; i++)
        RdJson::getLong("idx", 0, testJSON);
    EVAL_PERF_END(perfGetString1Us);

    // TEST_ASSERT_MESSAGE(true == testGetString("consts/lastly", "elephant", testJSON), "getString2");
    // TEST_ASSERT_MESSAGE(5 == RdJson::getLong("consts/comarr/[1]", -1, testJSON), "getLong1");

    // // Test array elements
    // const char* expectedStrs[] = {"6", "5", "4", "3", "3", "{\"fish\": \"stew\"}"};
    // TEST_ASSERT_MESSAGE(true == testGetArrayElems("consts/comarr", expectedStrs, sizeof(expectedStrs)/sizeof(expectedStrs[0]), testJSON), "getArrayElems1");

    // // Test object keys
    // const char* expectedKeys[] = {"axis", "oxis", "exis", "comarr", "lastly"};
    // TEST_ASSERT_MESSAGE(true == testGetObjectKeys("consts", expectedKeys, sizeof(expectedKeys)/sizeof(expectedKeys[0]), testJSON), "getKeys1");

    // Dump timings
    LOG_I(MODULE_PREFIX, "Parse %fms FindElemEnd %fms FindKey %fms GetStr1 %f", 
                perfParseUs/1000.0, perfFindElemEndUs/1000.0, perfFindKeyUs/1000.0, perfGetString1Us/1000.0/100.0);
}

TEST_CASE("test_rdjson_perf2", "[rdjsonperf]")
{
    testAddHwElemInfos();
    LOG_I(MODULE_PREFIX, "Loop100Ms %fms", perfLoop100Us/1000.0);
}
