/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Unit test of Expressions
//
// Rob Dobson 2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <limits.h>
#include "unity.h"
#include <ExpressionEval.h>
#include <Logger.h>

// #define DEBUG_EXPRESSIONS

static const char* MODULE_PREFIX = "ExprUnitTest";

ExpressionEval evaluator;

static bool checkVal(ExpressionEval& evaluator, const char* varName, double expectedVal)
{
    bool isValid;
    double val = evaluator.getVal(varName, isValid);
    if (!isValid)
    {
        LOG_I(MODULE_PREFIX, "checkVal name %s val not found", varName);
        return false;
    }
    if (val != expectedVal)
    {
        LOG_I(MODULE_PREFIX, "checkVal name %s expected %f != %f", varName, expectedVal, val);
        return false;
    }
    return true;
}

static const char* initJSON =
    R"({)"
        R"("minAngle":-180,)"
        R"("maxAngle":180,)"
        R"("moveTimeMin":400,)"
        R"("hipAngleMax":35,)"
        R"("stepLengthMax":100,)"
        R"("leanMultMax":1,)"
        R"("leftHip":0,)"
        R"("leftKnee":2,)"
        R"("DIR_BACK":1,)"
        R"("DIR_LEFT":0,)"
        R"("DIR_LEFT":0,)"
        R"("$persistentVar1":1073,)"
        R"("moveTime":1500,)"
        R"("leanAmount":30,)"
        R"("stepLength":50)"
    R"(})"
    ;

TEST_CASE("Expression test 1", "[expressions]")
{
    // Set values
    evaluator.clear();
    evaluator.addVariables(initJSON, false);

    // Check values
    struct TestElem
    {
        const char* varName;
        double expVal;
    };
    TestElem initialVarAssertions [] = {
        { "minAngle", -180 },
        { "maxAngle", 180 },
        { "moveTimeMin", 400 },
        { "moveTime", 1500 },
        { "hipAngleMax", 35 },
        { "stepLengthMax", 100 },
        { "leanMultMax", 1 },
        { "leftHip", 0 },
        { "leftKnee", 2 },
        { "DIR_BACK", 1 },
        { "DIR_LEFT", 0 },
        { "DIR_LEFT", 0 },
        { "leanAmount", 30 },
        { "stepLength", 50 },
        { "$persistentVar1", 1073 }
        };
    for (int testIdx = 0; testIdx < sizeof(initialVarAssertions)/sizeof(initialVarAssertions[0]); testIdx++)
    {
        String tokStartStr = "testExpressions initTestIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(true == checkVal(evaluator, initialVarAssertions[testIdx].varName, 
                        initialVarAssertions[testIdx].expVal), tokStartStr.c_str());
    }

    // Add expression
    uint32_t errorLine = 0;
    evaluator.addExpressions("a = max(moveTimeMin, moveTime) * 2\nnewVar=$persistentVar1 + 17", errorLine);
    evaluator.evalStatements("{}");
    TestElem finalVarAssertions [] = {
        { "minAngle", -180 },
        { "maxAngle", 180 },
        { "moveTimeMin", 400 },
        { "moveTime", 1500 },
        { "a", 3000 },
        { "hipAngleMax", 35 },
        { "stepLengthMax", 100 },
        { "leanMultMax", 1 },
        { "leftHip", 0 },
        { "leftKnee", 2 },
        { "DIR_BACK", 1 },
        { "DIR_LEFT", 0 },
        { "DIR_LEFT", 0 },
        { "leanAmount", 30 },
        { "stepLength", 50 },
        { "$persistentVar1", 1073 },
        { "newVar", 1073 + 17 }
    };
    for (int testIdx = 0; testIdx < sizeof(finalVarAssertions)/sizeof(finalVarAssertions[0]); testIdx++)
    {
        String tokStartStr = "testExpressions finalTestIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(true == checkVal(evaluator, finalVarAssertions[testIdx].varName, 
                        finalVarAssertions[testIdx].expVal), tokStartStr.c_str());
    }

}

static const char* initJSON2 =
    R"({)"
        R"("moveTimeMin":400,)"
        R"("hipAngleMax":35,)"
        R"("stepLengthMax":100,)"
        R"("leanMultMax":1,)"
        R"("leftHip":0,)"
        R"("leftKnee":2,)"
        R"("DIR_BACK":1,)"
        R"("DIR_LEFT":1,)"
        R"("moveTime":1500,)"
        R"("leanAmount":30,)"
        R"("stepLength":50)"
    R"(})"
    ;

static const char* testTraj1 =
R"(
moveTime = 100
moveTimeB = max(moveTime, moveTimeMin)
stepLength = min(stepLength, stepLengthMax)
leanMult = min(moveTimeB * 0.001, leanMultMax)
leftHipPos = servoPos(leftHip, 0.375, leftHipPos)
leftHipPos = servoPos(leftHip, 0.25, stepLength * hipAngleMax * DIR_BACK)
leftHipPos = servoPos(leftHip, 0.375, leftHipPos)
leftKneePos = servoPos(leftKnee, 0.25, leanAmount * leanMult * DIR_LEFT)
playSound(0.25, "firstSound")
playsound(0.75, "anotherSound")
)"
;

static double testServoPos(double a, double b, double c)
{
#ifdef DEBUG_EXPRESSIONS
    LOG_I(MODULE_PREFIX, "testServoPos %f %f %f", a, b, c);
#endif
    return c;
}

static double testPlaySound(double a, double b)
{
    // Get string constant
    String strConst = evaluator.getStringConst(b);
#ifdef DEBUG_EXPRESSIONS
    LOG_I(MODULE_PREFIX, "testPlaySound %f %s (idx %d)", a, strConst.c_str(), int(b));
#endif
    return 0;
}

TEST_CASE("Expression test 2", "[expressions]")
{
    // Set values
    evaluator.clear();
    evaluator.addVariables(initJSON2, false);
    uint32_t numLocalVars, numGlobalVars, numFuncs;
    evaluator.getNumVarsAndFuncs(numLocalVars, numGlobalVars, numFuncs);
    // LOG_I(MODULE_PREFIX, "numLocalVars %d numGlobalVars %d numFuncs %d", numLocalVars, numGlobalVars, numFuncs);
    TEST_ASSERT_MESSAGE(numLocalVars == 11, "Incorrect number of local vars");

    // Debug
    // evaluator.debugLogVars();
    
    // // Check values
    struct TestElem
    {
        const char* varName;
        double expVal;
    };
    TestElem initialVarAssertions [] = {
        { "moveTimeMin", 400 },
        { "DIR_LEFT", 1 },
    };
    for (int testIdx = 0; testIdx < sizeof(initialVarAssertions)/sizeof(initialVarAssertions[0]); testIdx++)
    {
        String tokStartStr = "testExpressions initTestIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(checkVal(evaluator, initialVarAssertions[testIdx].varName, 
                        initialVarAssertions[testIdx].expVal), tokStartStr.c_str());
    }

    // Add functions
    evaluator.addFunction("servoPos", testServoPos);
    evaluator.addFunction("playSound", testPlaySound);

    // Debug
    // evaluator.debugLogVars();

    // Add expressions
    uint32_t errorLine = 0;
    TEST_ASSERT_MESSAGE(evaluator.addExpressions(testTraj1, errorLine), "Add Expressions Failed");

    // Eval with no params
    evaluator.evalStatements("");

    // Assertions before params applied
    TestElem noParamVarAssertions [] = {
        { "leftHipPos", 1750 },
        { "leftKneePos", 12 },
        { "moveTimeMin", 400 },
        { "moveTime", 100 },
    };
    for (int testIdx = 0; testIdx < sizeof(noParamVarAssertions)/sizeof(noParamVarAssertions[0]); testIdx++)
    {
        String tokStartStr = "testExpressions finalTestIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(checkVal(evaluator, noParamVarAssertions[testIdx].varName, 
                        noParamVarAssertions[testIdx].expVal), tokStartStr.c_str());
    }

    // Add parameters
    const char* paramsJSON = R"({"moveTime":500})";
    evaluator.addVariables(paramsJSON, true);

    // Eval with params
    evaluator.evalStatements(paramsJSON);

    // Assertions after params applied
    TestElem finalVarAssertions [] = {
        { "leftHipPos", 1750 },
        { "leftKneePos", 15 },
        { "moveTimeMin", 400 },
        { "moveTime", 500 },
    };
    for (int testIdx = 0; testIdx < sizeof(finalVarAssertions)/sizeof(finalVarAssertions[0]); testIdx++)
    {
        String tokStartStr = "testExpressions finalTestIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(checkVal(evaluator, finalVarAssertions[testIdx].varName, 
                        finalVarAssertions[testIdx].expVal), tokStartStr.c_str());
    }

}

static const char* initJSON3 =
    R"({)"
    R"(})"
    ;

static const char* testTraj3 =
R"(
    a = 4
    b = 24
    c = 0
    d = 0
    while: a < b
      if: a * 2 > b
        c = c - 3
      else:
        if: d < 5
          c = c + 10
        else:
          c = c + 20
          if: c == 30
            e = 47
          end:
          f = 4 * (9 == 9)
        end:
        d = d + 2
      end:
      a = a + 1
    end:
    if: (f >= 4) && (f < 5)
      g = 92
    end:
)"
;

TEST_CASE("Expression test 3", "[expressions]")
{
    // Set values
    evaluator.clear();
    evaluator.addVariables(initJSON3, false);
    uint32_t numLocalVars, numGlobalVars, numFuncs;
    evaluator.getNumVarsAndFuncs(numLocalVars, numGlobalVars, numFuncs);
    TEST_ASSERT_MESSAGE(numLocalVars == 0, "Incorrect number of local vars");
    TEST_ASSERT_MESSAGE(numGlobalVars == 1, "Incorrect number of global vars");

    // Debug
    // evaluator.debugLogVars();
    
    // Add expressions
    uint32_t errorLine = 0;
    TEST_ASSERT_MESSAGE(evaluator.addExpressions(testTraj3, errorLine), "Add Expressions Failed");

    // Eval
    evaluator.evalStatements("");

    // Check values
    struct TestElem
    {
        const char* varName;
        double expVal;
    };
    TestElem varAssertions [] = {
        { "a", 24 },
        { "b", 24 },
        { "c", 117 },
        { "d", 18 },
        { "e", 47 },
        { "f", 4 },
        { "g", 92 },
    };
    for (int testIdx = 0; testIdx < sizeof(varAssertions)/sizeof(varAssertions[0]); testIdx++)
    {
        String tokStartStr = "testExpressions3 finalTestIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(checkVal(evaluator, varAssertions[testIdx].varName, 
                        varAssertions[testIdx].expVal), tokStartStr.c_str());
    }
}

static const char* testTraj4 =
R"(
    isDefined1 = defined($persistentVar1)
    isDefined2 = defined($persistentVar2)
    if: isDefined1
      $persistentVar1 = $persistentVar1 + 1
    else:
      $persistentVar1 = 123
    end:
    if: isDefined2
      $persistentVar2 = $persistentVar2 + 1
    else:
      $persistentVar2 = 12
    end:
    notANumber = nan
)"
;

TEST_CASE("Expression test 4", "[expressions]")
{
    // Set values
    evaluator.clear();
    evaluator.addVariables(initJSON2, false);
    uint32_t numLocalVars, numGlobalVars, numFuncs;
    evaluator.getNumVarsAndFuncs(numLocalVars, numGlobalVars, numFuncs);
    // LOG_I(MODULE_PREFIX, "numLocalVars %d numGlobalVars %d numFuncs %d", numLocalVars, numGlobalVars, numFuncs);
    TEST_ASSERT_MESSAGE(numLocalVars == 11, "Incorrect number of local vars");
    TEST_ASSERT_MESSAGE(numGlobalVars == 1, "Incorrect number of global vars");

    // Add expressions
    uint32_t errorLine = 0;
    TEST_ASSERT_MESSAGE(evaluator.addExpressions(testTraj4, errorLine), "Add Expressions Failed");

    // Eval
    evaluator.evalStatements("");

    // Check values
    struct TestElem
    {
        const char* varName;
        double expVal;
    };
    TestElem varAssertions [] = {
        { "isDefined1", 1 },
        { "isDefined2", 0 },
        { "$persistentVar1", 1074 },
        { "$persistentVar2", 12 }
    };
    for (int testIdx = 0; testIdx < sizeof(varAssertions)/sizeof(varAssertions[0]); testIdx++)
    {
        String tokStartStr = "testExpressions4 finalTestIdx=" + String(testIdx);
        TEST_ASSERT_MESSAGE(checkVal(evaluator, varAssertions[testIdx].varName, 
                        varAssertions[testIdx].expVal), tokStartStr.c_str());
    }

    // Check NAN
    bool isValid = false;
    double val = evaluator.getVal("notANumber", isValid);
    TEST_ASSERT_MESSAGE(isValid, "notANumber is not valid");
    TEST_ASSERT_MESSAGE(isnan(val), "notANumber is not NAN");

    // Check local and global vars
    evaluator.getNumVarsAndFuncs(numLocalVars, numGlobalVars, numFuncs);
    // LOG_I(MODULE_PREFIX, "numLocalVars %d numGlobalVars %d numFuncs %d", numLocalVars, numGlobalVars, numFuncs);
    TEST_ASSERT_MESSAGE(numLocalVars == 14, "Incorrect number of local vars");
    TEST_ASSERT_MESSAGE(numGlobalVars == 2, "Incorrect number of global vars");

}
