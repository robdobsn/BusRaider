/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SupervisorStats
// Statistics for supervisor of module execution
//
// Rob Dobson 2018-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Utils.h>
#include "ArduinoTime.h"
#include <ExecTimer.h>

#ifdef DEBUG_GLOBAL_VALUE
#define SUPERVISE_SERVICE_CALL(supervisor, idx, debugVal, func) { DEBUG_GLOB_VAR_NAME(debugVal) = idx; supervisor.execStarted(idx); func; supervisor.execEnded(idx); }
#else
#define SUPERVISE_SERVICE_CALL(supervisor, idx, debugVal, func) { supervisor.execStarted(idx); func; supervisor.execEnded(idx); }
#endif
// Supervisor stats
class SupervisorStats
{
public:
    SupervisorStats();
    void init();
    void add(const char *name);
    void execStarted(uint32_t modIdx);
    void execEnded(uint32_t modIdx);
    uint32_t getCount()
    {
        return _moduleList.size();
    }
    void outerLoopStarted();
    void outerLoopEnded();

    // Outer loop details
    uint32_t getOuterLoopUsMax()
    {
        return _outerLoopInfo._loopTimeMax;
    }
    uint32_t getOuterLoopUsAvg()
    {
        if (_outerLoopInfo._loopTimeAvgCount == 0)
            return 0;
        return _outerLoopInfo._loopTimeAvgSum / _outerLoopInfo._loopTimeAvgCount;
    }

    // Calculate supervisory stats
    void calculate();

    // Clear stats
    void clear();
    String getSummaryString();

private:

    // Module info
    class ModInfo
    {
    public:
        ModInfo(const char *modName)
        {
            _modName = modName;
        }
        String _modName;
        ExecTimer execTimer;
    };

    // Modules under supervision
    static const uint32_t MAX_MODULES = 50;
    std::vector<ModInfo> _moduleList;

    // Outer loop info - info about the overall loop around modules
    class OuterLoopInfo
    {
    public:
        OuterLoopInfo()
        {
            clear();
        }
        void clear()
        {
            _loopTimeAvgSum = 0;
            _loopTimeAvgCount = 0;
            _loopTimeMax = 0;
            _loopTimeMin = 0;
            _lastLoopStartMicros = 0;
        }
        void startLoop()
        {
            _lastLoopStartMicros = micros();
        }
        void endLoop()
        {
            int64_t loopTime = Utils::timeElapsed(micros(), _lastLoopStartMicros);
            _loopTimeAvgSum += loopTime;
            _loopTimeAvgCount++;
            if (_loopTimeMin > loopTime)
                _loopTimeMin = loopTime;
            if (_loopTimeMax < loopTime)
                _loopTimeMax = loopTime;
        }
    
        unsigned long _loopTimeAvgSum;
        unsigned long _loopTimeAvgCount;
        unsigned long _loopTimeMax;
        unsigned long _loopTimeMin;
        uint64_t _lastLoopStartMicros;
    };
    OuterLoopInfo _outerLoopInfo;    

    // Summary info
    class SummaryInfo
    {
    public:
        SummaryInfo(uint32_t numSlowestToTrack)
        {
            _nThSlowestModIdxVec.resize(numSlowestToTrack);
            clear();
        }
        void clear()
        {
            for (auto& modIdx: _nThSlowestModIdxVec)
                modIdx = -1;
            _loopTimeMinUs = 0;
            _loopTimeMaxUs = 0;
            _loopTimeAvgUs = 0;
            _totalLoops = 0;
        }
        void updateOuterLoopStats(OuterLoopInfo& outerLoopInfo)
        {
            _totalLoops = outerLoopInfo._loopTimeAvgCount;
            _loopTimeMinUs = outerLoopInfo._loopTimeMin;
            _loopTimeMaxUs = outerLoopInfo._loopTimeMax;
            _loopTimeAvgUs = 0;
            if (outerLoopInfo._loopTimeAvgCount > 0)
                _loopTimeAvgUs = (1.0 * outerLoopInfo._loopTimeAvgSum) / outerLoopInfo._loopTimeAvgCount;
        }
        void updateSlowestModules(const std::vector<ModInfo>& moduleList);

        // Slowest modules list
        std::vector<int> _nThSlowestModIdxVec;

        // Outer loop timing
        unsigned long _loopTimeMinUs;
        unsigned long _loopTimeMaxUs;
        unsigned long _totalLoops;
        double _loopTimeAvgUs;
    };
    static const uint32_t NUM_SLOWEST_TO_TRACK = 2;
    SummaryInfo _summaryInfo;
};
