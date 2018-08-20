// RinggitTheSalesBell
// Rob Dobson 2018

#pragma once

class SystemDebug
{
  public:
    int _debugMode;
    SystemDebug()
    {
        _debugMode = 0;
    }
    int getMode()
    {
        return _debugMode;
    }
    void setMode(int debugMode)
    {
        _debugMode = debugMode;
    }
};

