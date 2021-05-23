/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SupervisorStats
// Statistics for supervisor of module execution
//
// Rob Dobson 2018-2021
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SupervisorStats.h"
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SupervisorStats::SupervisorStats() : _summaryInfo(NUM_SLOWEST_TO_TRACK)
{
    init();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::init()
{
    _summaryInfo.clear();
    _outerLoopInfo.clear();
    _moduleList.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear stats
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::clear()
{
    _summaryInfo.clear();
    _outerLoopInfo.clear();
    for (ModInfo &modInfo : _moduleList)
    {
        modInfo.execTimer.clear();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Add a module
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::add(const char *name)
{
    if (_moduleList.size() > MAX_MODULES)
        return;
    ModInfo modInfo(name);
    _moduleList.push_back(modInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Execution of a module started
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::execStarted(uint32_t modIdx)
{
    if (modIdx >= _moduleList.size())
        return;
    _moduleList[modIdx].execTimer.started();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Execution of a module ended
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::execEnded(uint32_t modIdx)
{
    if (modIdx >= _moduleList.size())
        return;
    _moduleList[modIdx].execTimer.ended();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Outer loop start
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::outerLoopStarted()
{
    _outerLoopInfo.startLoop();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Outer loop end
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::outerLoopEnded()
{
    _outerLoopInfo.endLoop();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate supervisory stats
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::calculate()
{
    // Calculate outer loop times
    _summaryInfo.updateOuterLoopStats(_outerLoopInfo);

    // Calculate slowest modules
    _summaryInfo.updateSlowestModules(_moduleList);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get summary string
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String SupervisorStats::getSummaryString()
{
    // Average loop time
    String outerLoopStr;
    if (_summaryInfo._totalLoops > 0)
    {
        char outerLoopTmp[150];
        snprintf(outerLoopTmp, sizeof(outerLoopTmp), "Avg %4.2fuS Max %luuS Min %luuS ", 
                _summaryInfo._loopTimeAvgUs,
                _summaryInfo._loopTimeMaxUs,
                _summaryInfo._loopTimeMinUs);
        outerLoopStr = outerLoopTmp;
    }

    // Find slowest modules
    char slowestStr[100];
    strlcpy(slowestStr, "", sizeof(slowestStr));

    // Slowest loop activity
    bool isFirst = true;
    for (int modIdx : _summaryInfo._nThSlowestModIdxVec)
    {
        if ((modIdx < 0) || (modIdx > _moduleList.size()))
            break;
        if (!_moduleList[modIdx].execTimer.valid())
            break;
        uint32_t strPos = strlen(slowestStr);
        if (sizeof(slowestStr) <= strPos + 1)
            break;
        snprintf(slowestStr + strPos, sizeof(slowestStr) - strPos,
                 isFirst ? "Slow %s %llduS" : ", %s %llduS",
                 _moduleList[modIdx]._modName.c_str(),
                 _moduleList[modIdx].execTimer.getMaxUs());
        isFirst = false;
    }

    // Combine
    return outerLoopStr + slowestStr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update slowest modules
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SupervisorStats::SummaryInfo::updateSlowestModules(const std::vector<ModInfo>& moduleList)
{
    // Fill list of slowest modules with invalid values initially
    for (uint32_t nthIdx = 0; nthIdx < _nThSlowestModIdxVec.size(); nthIdx++)
        _nThSlowestModIdxVec[nthIdx] = -1;

    // Fill list in order
    for (uint32_t nthIdx = 0; nthIdx < _nThSlowestModIdxVec.size(); nthIdx++)
    {
        // Find nth slowest
        int bestIdx = -1;
        uint32_t bestTimeUs = UINT32_MAX;
        for (uint32_t modIdx = 0; modIdx < moduleList.size(); modIdx++)
        {
            // Check if already in the list
            bool alreadyInList = false;
            for (uint32_t chkIdx = 0; chkIdx < _nThSlowestModIdxVec.size(); chkIdx++)
            {
                if (_nThSlowestModIdxVec[chkIdx] < 0)
                    break;
                if (_nThSlowestModIdxVec[chkIdx] == modIdx)
                {
                    alreadyInList = true;
                    break;
                }
            }
            if (alreadyInList)
                continue;

            // Check if better (slower) than previous
            if ((bestIdx == -1) || (bestTimeUs < moduleList[modIdx].execTimer.getMaxUs()))
            {
                bestIdx = modIdx;
                bestTimeUs = moduleList[modIdx].execTimer.getMaxUs();
            }
        }
        if (bestIdx < 0)
            break;

        // Store
        _nThSlowestModIdxVec[nthIdx] = bestIdx;
    }

}
