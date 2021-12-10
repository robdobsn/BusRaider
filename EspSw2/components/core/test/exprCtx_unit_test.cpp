/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit test of ExpressionContext
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <limits.h>
#include "unity.h"
#include <ExpressionContext.h>
#include <Logger.h>
#include <Utils.h>
#include <ArduinoTime.h>
#include "esp_heap_caps.h"

// #define DEBUG_EXPR_CTX_UNIT_TEST
// #define TEST_PERFORMANCE 1

#if defined(DEBUG_EXPR_CTX_UNIT_TEST) || defined(TEST_PERFORMANCE)
static const char* MODULE_PREFIX = "ExprCtxUnitTest";
#endif

#ifdef TEST_PERFORMANCE
static uint32_t perfStartTimeUs;
static uint64_t perfAddVarsUs;
#define EVAL_PERF_CLEAR(SVar) SVar = 0
#define EVAL_PERF_START() perfStartTimeUs = micros();
#define EVAL_PERF_ACCUM(SVar) {SVar += (micros() - perfStartTimeUs);}
#define EVAL_PERF_END(SVar) {SVar = (micros() - perfStartTimeUs);}
#else
#define EVAL_PERF_CLEAR(SVar) 
#define EVAL_PERF_START() 
#define EVAL_PERF_ACCUM(SVar) 
#define EVAL_PERF_END(SVar)
#endif

ExpressionContext exprContext;

struct NameValueDouble
{
    const char* name;
    double val;
};

NameValueDouble testVars1[] = {
    { "longVariableName", 12345 },
    { "klasjdlkasjd", 88 },
    { "adlasdfkler", 192 },
    { "klajsdnljkas", 8.124343425 },
    { "asdfisdafsds", 18273823 },
    { "$globaltest1", 9867 },
    { "akjanrjknjka", 9.3323 }
};
static const uint32_t numTestVars1 = sizeof(testVars1)/sizeof(testVars1[0]);

TEST_CASE("ExprCtx 1", "[expressionCtx]")
{
    EVAL_PERF_START();
    // Get value of global variable
    bool isValid = false;
    double val = exprContext.getVal("$globaltest1", isValid);
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(NAN, val, "Wrong value of $globaltest1");
    TEST_ASSERT_MESSAGE(isValid, "Expected $globaltest1 to be valid");

    // Set values
    for (int i = 0; i < numTestVars1; i++)
        exprContext.addVariable(testVars1[i].name, testVars1[i].val);
    EVAL_PERF_END(perfAddVarsUs);

    // Check values
    for (int i = 0; i < numTestVars1; i++)
    {
        String testMsg = "testVars1_" + String(i);
        val = exprContext.getVal(testVars1[i].name, isValid);
#ifdef DEBUG_EXPR_CTX_UNIT_TEST
        LOG_I(MODULE_PREFIX, "%s val %f isValid %d", testVars1[i].name, val, isValid);
#endif
        TEST_ASSERT_MESSAGE(isValid && (val == testVars1[i].val), testMsg.c_str());
    }

    // Dump
#ifdef DEBUG_EXPR_CTX_UNIT_TEST
    LOG_I(MODULE_PREFIX, "Dump Before Clear");
    exprContext.debugLogVars();
#endif

    // Cleanup
    exprContext.clear();

    // Dump
#ifdef DEBUG_EXPR_CTX_UNIT_TEST
    LOG_I(MODULE_PREFIX, "Dump After Clear");
    exprContext.debugLogVars();
#endif

    // Check global variable value
    val = exprContext.getVal("$globaltest1", isValid);
    TEST_ASSERT_MESSAGE(9867 == val, "Expected $globaltest1 value to be 9867");
    TEST_ASSERT_MESSAGE(isValid, "Expected $globaltest1 to be valid");

#ifdef TEST_PERFORMANCE
    // Performance
    LOG_I(MODULE_PREFIX, "AddVars %fms", perfAddVarsUs/1000.0);
#endif
}

static const uint32_t numTestLoops2 = 10;
static const uint32_t numTestVars2 = 1000;
String testVarNames2[numTestVars2];
double testVarVals2[numTestVars2];
TEST_CASE("ExprCtx 2", "[expressionCtx]")
{
    // Setup
    exprContext.clear();
    for (int i = 0; i < numTestVars2; i++)
    {
        testVarNames2[i] = "testVal" + String(i);
        testVarVals2[i] = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    }

#ifdef TEST_PERFORMANCE
    uint32_t freeHeapStart = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t freeHeapEnd = 0;
#endif
    EVAL_PERF_CLEAR(perfAddVarsUs);
    for (int j = 0; j < numTestLoops2; j++)
    {
        EVAL_PERF_START();
        // Add vars
        for (int i = 0; i < numTestVars2; i++)
        {
            exprContext.addVariable(testVarNames2[i].c_str(), testVarVals2[i]);
        }
        EVAL_PERF_ACCUM(perfAddVarsUs);

        // Re-add a var to check we overwrite an existing variable rather than creating two of them
        exprContext.addVariable(testVarNames2[1].c_str(), testVarVals2[1]);

        // Check values
        for (int i = 0; i < numTestVars2; i++)
        {
            String testMsg = "testVars2_" + String(i);
            bool isValid = false;
            double val = exprContext.getVal(testVarNames2[i].c_str(), isValid);
            TEST_ASSERT_MESSAGE(isValid && (val == testVarVals2[i]), testMsg.c_str());
        }

#ifdef TEST_PERFORMANCE
        if (j == numTestLoops2-1)
        {
            freeHeapEnd = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            // Dump
            // exprContext.debugLogVars();
        }
#endif

        // Check count
        uint32_t numLocalVars = 0, numGlobalVars = 0, numFuncs = 0;
        exprContext.getNumVarsAndFuncs(numLocalVars, numGlobalVars, numFuncs);
        String testMsg = "testVars2Counts";
        TEST_ASSERT_MESSAGE(numLocalVars == numTestVars2, (testMsg + "LocalVars").c_str());
        TEST_ASSERT_MESSAGE(numGlobalVars == 1, (testMsg + "GlobalVars").c_str());
        TEST_ASSERT_MESSAGE(numFuncs == 0, (testMsg + "Funcs").c_str());

        // Cleanup
        exprContext.clear();
    }

#ifdef TEST_PERFORMANCE
    uint32_t freeHeapCleanedUp = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif

#ifdef TEST_PERFORMANCE
    // Performance
    LOG_I(MODULE_PREFIX, "AddVars perVar %fus %fbytes total %fms heapLeakage %d heapUse %d startHeap %d endHeap %d cleanedUp %d", 
                perfAddVarsUs/(double)numTestVars2/numTestLoops2, (freeHeapStart-freeHeapEnd)/(double)numTestVars2,
                perfAddVarsUs/1000.0/numTestLoops2, 
                freeHeapStart-freeHeapCleanedUp,
                freeHeapStart-freeHeapEnd, freeHeapStart, freeHeapEnd, freeHeapCleanedUp);
#endif
}
