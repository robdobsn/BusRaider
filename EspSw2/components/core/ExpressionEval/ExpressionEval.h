/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExpressionEval
// Handles mathematical expressions with variables
//
// Rob Dobson 2017-2020
// Originally from RBotFirmware
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ExpressionContext.h"
#include "tinyexpr.h"
#include <vector>
#include <ConfigBase.h>
#include <Utils.h>

// Expression custom functions
typedef double (*ExprCustomFn0DRD)();
typedef double (*ExprCustomFn1DRD)(double a);
typedef double (*ExprCustomFn2DRD)(double a, double b);
typedef double (*ExprCustomFn3DRD)(double a, double b, double c);
typedef double (*ExprCustomFn4DRD)(double a, double b, double c, double d);


class ExpressionEval
{
public:
    ExpressionEval();
    ~ExpressionEval();
    void clear();

    // Set vals
    void addVariables(const char* valsJSON, bool append);
    void addVariables(std::vector<NameValuePairDouble>& nameValuePairs, bool append);
    void addVariable(const char* varName, double val)
    {
        _exprContext.addVariable(varName, val);
    }
    
    // Custom functions
    void addFunction(const char* fnName, ExprCustomFn0DRD pFn);
    void addFunction(const char* fnName, ExprCustomFn1DRD pFn);
    void addFunction(const char* fnName, ExprCustomFn2DRD pFn);
    void addFunction(const char* fnName, ExprCustomFn3DRD pFn);
    void addFunction(const char* fnName, ExprCustomFn4DRD pFn);

    // Expressions
    bool addExpressions(const char* exprStr, uint32_t& errorLine);
    void evalStatements(const char* immutableVarsJSON);

    // Access values
    double getVal(const char* varName, bool& isValid)
    {
        return _exprContext.getVal(varName, isValid);
    }

    // Get string const
    String getStringConst(int constIdx)
    {
        if ((constIdx < 0) || (constIdx >= _stringConsts.size()))
            return "";
        return _stringConsts[constIdx];
    }

    // Get numbers of vars and functions
    void getNumVarsAndFuncs(uint32_t& numVars, uint32_t& numFuncs)
    {
        return _exprContext.getNumVarsAndFuncs(numVars, numFuncs);
    }

    // Debug
    void debugLogVars()
    {
        _exprContext.debugLogVars();
    }

private:

    // All variables used in the expressions
    ExpressionContext _exprContext;

    // Flow control type
    enum StatementFlowType 
    {
        FLOW_TYPE_NONE,
        FLOW_TYPE_IF,
        FLOW_TYPE_ELSE,
        FLOW_TYPE_WHILE,
        FLOW_TYPE_END,
    };

    // Store for a compiled statement which can contain an expression and involve assignment
    // or flow control
    class CompiledStatement
    {
    public:
        CompiledStatement()
        {
            _pCompExpr = NULL;
            _flowType = FLOW_TYPE_NONE;
        };
        te_expr* _pCompExpr;
        String _assignedVarName;
        StatementFlowType _flowType;
    };

    // List of compiled expressions and assignments
    static const uint32_t MAX_VAR_LEN = 30;
    static const uint32_t MAX_EXPR_LEN = 500;
    static const uint32_t MAX_EXPRESSIONS = 100;
    static const uint32_t MAX_EXPRESSION_EVAL_PROC_LINES = 5000;
    std::vector<CompiledStatement> _compiledStatements;

    // Vector of string constants
    std::vector<String> _stringConsts;

    // Helpers
    void findAndReplaceStringConsts(String& exprStr);
    void handleExpressions(const char* pExpr, bool addVars, bool compileExprs);
    bool compileAndStore(String& expr, const String& varName, StatementFlowType flowType);
    uint32_t findMatchingFlowUnit(uint32_t pc);
    const char* getFlowTypeStr(StatementFlowType flowType)
    {
        switch(flowType)
        {
            default:
            case FLOW_TYPE_NONE: return "NONE";
            case FLOW_TYPE_IF: return "IF";
            case FLOW_TYPE_ELSE: return "ELSE";
            case FLOW_TYPE_END: return "END";
            case FLOW_TYPE_WHILE: return "WHILE";
        }
    }
};
