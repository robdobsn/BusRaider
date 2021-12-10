/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExpressionContext
// Handles variables for ExpressionEval
//
// Rob Dobson 2017-2021
// Originally from RBotFirmware
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ExpressionContext.h"
#include <Logger.h>
#include "math.h"

// #define DEBUG_EVALUATOR_EXPRESSIONS 1

// #define OPTIMIZE_ASSIGNMENT_REALLOC 1

static const char* MODULE_PREFIX = "ExprCtx";

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / Destruction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ExpressionContext::ExpressionContext()
{
    _teVarsAllocated = false;
}

ExpressionContext::~ExpressionContext()
{
    clear(true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clean up
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ExpressionContext::clear(bool includeGlobals)
{
    // Clear variables
    if (includeGlobals)
    {
        // Clear all variables including globals
        _mapVars.clear();
    }
    else
    {
        // Make a copy of the globals
        std::map<String, double> mapGlobals;
        for (auto it = _mapVars.begin(); it != _mapVars.end(); ++it)
        {
            if (it->first.startsWith(GLOBAL_VAR_PREFIX))
                mapGlobals[it->first] = it->second;
        }

        // Clear all variables
        _mapVars.clear();

        // Restore globals
        _mapVars = mapGlobals;
    }

    // Clear functions
    _mapFuncs.clear();

    // Clear tinyexpression variables
    _teVars.clear();
}

void ExpressionContext::clearTEVars()
{
    _teVars.clear();
    _teVarsAllocated = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add variable
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ExpressionContext::addVariable(const char* name, double val, bool overwriteValue)
{
    // Find existing variable
    std::map<String, double>::iterator pos = _mapVars.find(name);
    if (pos == _mapVars.end())
    {
        // Not found so add the value
        _mapVars[name] = val;

#ifdef OPTIMIZE_ASSIGNMENT_REALLOC
        // Check if we need to update _teVars
        if (_teVarsAllocated)
        {
            pos = _mapVars.find(name);
            if (pos != _mapVars.end())
                setTEVar(name, &(pos->second));
        }
#endif

    }
    else
    {
        // Exists so just change the value if needed
        if (overwriteValue)
            pos->second = val;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add function
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ExpressionContext::addFunction(const char* name, const void* pFn, uint32_t numFunctionParams)
{
    // Find existing function
    std::map<String, FnDefStruct>::iterator pos = _mapFuncs.find(name);
    if (pos == _mapFuncs.end())
    {
        // Not found so add the function
        FnDefStruct fnDef = {pFn, numFunctionParams};
        _mapFuncs[name] = fnDef;

#ifdef OPTIMIZE_ASSIGNMENT_REALLOC
        // And update _teVars if needed
        if (_teVarsAllocated)
        {
            pos = _mapFuncs.find(name);
            if (pos != _mapFuncs.end())
                setTEFunc(name, pos->second.fnPtr, pos->second.numArgs);
        }
#endif
    }
    else
    {
        // Exists - let's ignore this as it shouldn't happen
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get value
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double ExpressionContext::getVal(const char* varName, bool& isValid)
{
    // Check for global value assumes the prefix is a single character (it is ok because both are null terminated strings)
    isValid = false;
    double retVal = 0;
    if (varName[0] == GLOBAL_VAR_PREFIX[0])
    {
        // Default to valid and not-a-number (NaN) if a global variable is requested and not defined
        isValid = true;
        retVal = NAN;
    }

    // Get the value
    std::map<String, double>::const_iterator pos = _mapVars.find(varName);
    if (pos == _mapVars.end())
        return retVal;
    isValid = true;
    return pos->second;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get tinyexpr vars
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ExpressionContext::getTEVars(std::vector<te_variable>& vars)
{
#ifdef OPTIMIZE_ASSIGNMENT_REALLOC
     // Check if we've already allocated (lazy allocation)
    if (_teVarsAllocated)
    {
#ifdef DEBUG_EVALUATOR_EXPRESSIONS
        LOG_I(MODULE_PREFIX, "getTEVars already allocated");
#endif
        // Return vars
        vars = _teVars;
        return;
    }
#endif

    // Resize the variable vector and allocates space for items plus some extra
    // for variables to be added later (expression assignments for instance)
    uint32_t numVarsAndFuncs = _mapVars.size() + _mapFuncs.size();
    _teVars.resize(0);
    _teVars.reserve(numVarsAndFuncs + ADDITIONAL_TEVARS_TO_RESERVE);
    // Populate variables
    for (std::map<String, double>::iterator itVar = _mapVars.begin(); itVar != _mapVars.end(); itVar++)
        setTEVar(itVar->first.c_str(), &(itVar->second));

    // Populate functions
    for (std::map<String, FnDefStruct>::iterator itFunc = _mapFuncs.begin(); itFunc != _mapFuncs.end(); itFunc++)
        setTEFunc(itFunc->first.c_str(), itFunc->second.fnPtr, itFunc->second.numArgs);

    // From now on auto-update TE Vars
    _teVarsAllocated = true;

    // Debug
#ifdef DEBUG_EVALUATOR_EXPRESSIONS
    LOG_I(MODULE_PREFIX, "getTEVars numVars %d numFuncs %d", _mapVars.size(), _mapFuncs.size());
#endif
    // Return vars
    vars = _teVars;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ExpressionContext::debugLogVars()
{
    // Dump vars
    for (std::pair<String, double> el : _mapVars)
    {
        LOG_I(MODULE_PREFIX, "debugLogVars name %s val %f", 
                    el.first.c_str(), el.second);
    }

    // Dump funcs
    for (std::pair<String, FnDefStruct> el : _mapFuncs)
    {
        LOG_I(MODULE_PREFIX, "debugLogVars func name %s args %d", 
                el.first.c_str(), el.second.numArgs);
    }
}

const char* ExpressionContext::getVarTypeStr(int type)
{
    switch(type)
    {
        case TE_VARIABLE: return "var";
        case TE_FUNCTION0:
        case TE_FUNCTION1:
        case TE_FUNCTION2:
        case TE_FUNCTION3:
        case TE_FUNCTION4:
        case TE_FUNCTION5:
        case TE_FUNCTION6:
        case TE_FUNCTION7: return "func";
        case TE_CLOSURE0:
        case TE_CLOSURE1:
        case TE_CLOSURE2:
        case TE_CLOSURE3:
        case TE_CLOSURE4:
        case TE_CLOSURE5:
        case TE_CLOSURE6:
        case TE_CLOSURE7: return "clos";
        default: return "unkn";
    }
}
