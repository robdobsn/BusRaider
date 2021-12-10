/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExpressionContext
// Handles variables for ExpressionEval
//
// Rob Dobson 2017-2021
// Originally from RBotFirmware
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "tinyexpr.h"
#include <WString.h>
#include <vector>
#include <map>

class ExpressionContext
{
public:
    ExpressionContext();
	~ExpressionContext();

    // Global variable prefix
    static constexpr const char* GLOBAL_VAR_PREFIX = "$";

    // Add varibles and functions
    void addVariable(const char* name, double val, bool overwriteValue = true);
    void addFunction(const char* name, const void* pFn, uint32_t numFunctionParams);

    // Get
	double getVal(const char* varName, bool& isValid);
    
    // Get TE Vars (for tinyexpr)
    void getTEVars(std::vector<te_variable>& vars);

    // Get numbers of vars and functions
    void getNumVarsAndFuncs(uint32_t& numLocalVars, uint32_t& numGlobalVars, uint32_t& numFuncs)
    {
        // Count global variables
        numGlobalVars = 0;
        for (auto it = _mapVars.begin(); it != _mapVars.end(); ++it)
        {
            if (it->first.startsWith(GLOBAL_VAR_PREFIX))
                numGlobalVars++;
        }
        numLocalVars = _mapVars.size() - numGlobalVars;
        numFuncs = _mapFuncs.size();
    }

    // Clear
	void clear(bool includeGlobals = false);
    void clearTEVars();

    // Debug
    void debugLogVars();
    static const char* getVarTypeStr(int type);

private:
    // Helpers
    void splitAssignmentExpr(const char* inStr, String& varName, String& expr);
    void setTEVar(const char* pName, const void* pAddress)
    {
        te_variable newVar = {pName, pAddress, TE_VARIABLE, NULL};
        _teVars.push_back(newVar);
    }
    void setTEFunc(const char* pName, const void* pFnPtr, int numVars)
    {
        te_variable newVar = {pName, pFnPtr, TE_FUNCTION0 + numVars, NULL};
        _teVars.push_back(newVar);
    }

    // Variables map
    std::map<String, double> _mapVars;

    // Functions map
    struct FnDefStruct
    {
        const void* fnPtr;
        uint32_t numArgs;
    };
    std::map<String, FnDefStruct> _mapFuncs;

    // Tinyexpr variables
    static const uint32_t ADDITIONAL_TEVARS_TO_RESERVE = 10;
    bool _teVarsAllocated;
    std::vector<te_variable> _teVars;    
};
