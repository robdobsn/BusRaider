#pragma once

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Status Info
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BusAccessStats
{
public:
    BusAccessStats()
    {
        clear();
    }
    
    void clear()
    {
        isrCount = 0;
        isrAccumUs = 0;
        isrAvgingCount = 0;
        isrAvgNs = 0;
        isrMaxUs = 0;
        isrSpuriousBUSRQ = 0;
        isrDuringBUSACK = 0;
        isrWithoutWAIT = 0;
        clrAccumUs = 0;
        clrAvgingCount = 0;
        clrAvgNs = 0;
        clrMaxUs = 0;
        busrqFailCount = 0;
        busActionFailedDueToWait = 0;
        isrMREQRD = 0;
        isrMREQWR = 0;
        isrIORQRD = 0;
        isrIORQWR = 0;
        isrIRQACK = 0;
#ifdef DEBUG_IORQ_PROCESSING
        _debugIORQNum = 0;
        _debugIORQClrMicros = 0;
        _debugIORQIntProcessed = false;
        _debugIORQIntNext = false;
        _debugIORQMatchNum = 0;
        _debugIORQMatchExpected = 0;
#endif
    }

    static const int MAX_JSON_LEN = 30 * 20;
    static char _jsonBuf[MAX_JSON_LEN];
    const char* getJson();

    // Overall ISR
    uint32_t isrCount;

    // ISR elapsed
    uint32_t isrAccumUs;
    int isrAvgingCount;
    int isrAvgNs;
    uint32_t isrMaxUs;

    // Counts
    uint32_t isrSpuriousBUSRQ;
    uint32_t isrDuringBUSACK;
    uint32_t isrWithoutWAIT;
    uint32_t isrMREQRD;
    uint32_t isrMREQWR;
    uint32_t isrIORQRD;
    uint32_t isrIORQWR;
    uint32_t isrIRQACK;

    // Clear pulse edge width
    uint32_t clrAccumUs;
    int clrAvgingCount;
    int clrAvgNs;
    uint32_t clrMaxUs;

    // BUSRQ
    uint32_t busrqFailCount;

    // Bus actions
    uint32_t busActionFailedDueToWait;

#ifdef DEBUG_IORQ_PROCESSING
    class DebugIORQ
    {
    public:
        uint32_t _micros;
        uint8_t _addr;
        uint8_t _data;
        uint16_t _flags;
        uint32_t _count;

        DebugIORQ() 
        {
            _micros = 0;
            _addr = 0;
            _data = 0;
            _flags = 0;
            _count = 0;
        }
    };

    static const int MAX_DEBUG_IORQ_EVS = 20;
    DebugIORQ _debugIORQEvs[MAX_DEBUG_IORQ_EVS];
    int _debugIORQNum;
    uint32_t _debugIORQClrMicros;
    bool _debugIORQIntProcessed;
    bool _debugIORQIntNext;

    DebugIORQ _debugIORQEvsMatch[MAX_DEBUG_IORQ_EVS];
    int _debugIORQMatchNum;
    int _debugIORQMatchExpected;
#endif

};