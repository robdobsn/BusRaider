/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// JSON Params
// Holder for JSON information providing access methods
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ConfigBase.h"

class JSONParams : public ConfigBase
{
public:
    JSONParams(const char* configStr) :
        ConfigBase(configStr)
    {
    }

    JSONParams(const String& configStr) :
        ConfigBase(configStr)
    {
    }

    const char* c_str()
    {
        return _dataStrJSON.c_str();
    }

    const String& configStr()
    {
        return _dataStrJSON;
    }
};
