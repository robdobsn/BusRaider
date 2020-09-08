/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RingBufferPosnRTOS
// Generic ring buffer pointer class
// Each pointer is only updated by one source
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class RingBufferPosnRTOS
{
public:
    volatile unsigned int _putPos;
    volatile unsigned int _getPos;
    unsigned int _bufLen;
    SemaphoreHandle_t _rbMutex;
    static const int PUT_TICKS_TO_WAIT = 1;
    static const int GET_TICKS_TO_WAIT = 1;
    static const int CLEAR_TICKS_TO_WAIT = 2;

    RingBufferPosnRTOS(int maxLen)
    {
        init(maxLen);
        _rbMutex = xSemaphoreCreateMutex();
    }

    void init(int maxLen)
    {
        _bufLen = maxLen;
        _putPos = 0;
        _getPos = 0;
    }

    void clear()
    {
        if (xSemaphoreTake(_rbMutex, CLEAR_TICKS_TO_WAIT) == pdTRUE)
        {
            _getPos = _putPos = 0;
            xSemaphoreGive(_rbMutex);
        }
    }

    inline unsigned int posToGet()
    {
        return _getPos;
    }

    inline unsigned int posToPut()
    {
        return _putPos;
    }

    // If true is returned then the semaphore has been acquired and hasPut must be called
    // to return the semaphore
    bool canPut()
    {
        // Validity check - zero length buffer
        if (_bufLen == 0)
            return false;

        // Acquire semaphore
        if (xSemaphoreTake(_rbMutex, PUT_TICKS_TO_WAIT) == pdTRUE)
        {
            // Check space available
            if (_putPos == _getPos)
                return true;
            unsigned int gp = _getPos;
            if (_putPos > gp)
            {
                if ((_putPos != _bufLen - 1) || (gp != 0))
                    return true;
            }
            else
            {
                if (gp - _putPos > 1)
                    return true;
            }
            // Return semaphore - no space
            xSemaphoreGive(_rbMutex);
        }
        return false;
    }

    // If true is returned then the semaphore has been acquired and hasGot must be called
    // to return the semaphore
    bool canGet()
    {
        // Acquire semaphore
        if (xSemaphoreTake(_rbMutex, GET_TICKS_TO_WAIT) == pdTRUE)
        {
            bool retVal = _putPos != _getPos;
            if (!retVal)
            {
                // Return semaphore - nothing available
                xSemaphoreGive(_rbMutex);
            }
            return retVal;
        }
        return false;
    }

    void hasPut()
    {
        // Put
        _putPos++;
        if (_putPos >= _bufLen)
            _putPos = 0;
        
        // Return the semaphore
        xSemaphoreGive(_rbMutex);
    }

    void hasGot()
    {
        // Get
        _getPos++;
        if (_getPos >= _bufLen)
            _getPos = 0;

        // Return the semaphore
        xSemaphoreGive(_rbMutex);
    }

    unsigned int count()
    {
        unsigned int retVal = 0;
        if (xSemaphoreTake(_rbMutex, GET_TICKS_TO_WAIT) == pdTRUE)
        {
            unsigned int posToGet = _getPos;
            if (posToGet <= _putPos)
                retVal = _putPos - posToGet;
            else
                retVal = _bufLen - posToGet + _putPos;
            xSemaphoreGive(_rbMutex);
        }
        return retVal;
    }

    // Get Nth element prior to the put position
    // 0 is the last element put in the queue
    // 1 is the one put in before that
    // Returns -1 if invalid
    int getNthFromPut(unsigned int N)
    {
        if (!canGet())
            return -1;
        if (N >= _bufLen)
            return -1;
        int nthPos = _putPos - 1 - N;
        if (nthPos < 0)
            nthPos += _bufLen;
        if (((unsigned int)(nthPos + 1) == _getPos) || ((unsigned int)(nthPos + 1) == _bufLen && _getPos == 0))
            return -1;
        return nthPos;
    }

    // Get Nth element from the get position
    // 0 is the element next got from the queue
    // 1 is the one got after that
    // returns -1 if invalid
    int getNthFromGet(unsigned int N)
    {
        if (!canGet())
            return -1;
        if (N >= _bufLen)
            return -1;
        unsigned int nthPos = _getPos + N;
        if (nthPos >= _bufLen)
            nthPos -= _bufLen;
        if (nthPos == _putPos)
            return -1;
        return nthPos;
    }
};
