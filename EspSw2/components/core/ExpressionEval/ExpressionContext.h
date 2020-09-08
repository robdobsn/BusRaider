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

#include "tinyexpr.h"
#include <WString.h>
#include <vector>
#include <map>

class ExpressionContext
{
public:
    ExpressionContext();
	~ExpressionContext();

    // Add varibles and functions
    void addVariable(const char* name, double val, bool overwriteValue = true);
    void addFunction(const char* name, const void* pFn, uint32_t numFunctionParams);

    // Get
	double getVal(const char* varName, bool& isValid);
    
    // Get TE Vars (for tinyexpr)
    void getTEVars(std::vector<te_variable>& vars);

    // Get numbers of vars and functions
    void getNumVarsAndFuncs(uint32_t& numVars, uint32_t& numFuncs)
    {
        numVars = _mapVars.size();
        numFuncs = _mapFuncs.size();
    }

    // Clear
	void clear();
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
