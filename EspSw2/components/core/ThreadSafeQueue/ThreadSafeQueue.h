/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ThreadSafeQueue
// Template-based queue
//
// Rob Dobson 2012-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <queue>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

template<typename ElemT>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue(uint32_t maxLen = DEFAULT_MAX_QUEUE_LEN)
    {
        // Mutex for ThreadSafeQueue
        _queueMutex = xSemaphoreCreateMutex();
        _maxLen = maxLen;
        _maxTicksToWaitDefault = DEFAULT_MAX_MS_TO_WAIT;
    }

    virtual ~ThreadSafeQueue()
    {
        if (_queueMutex)
            vSemaphoreDelete(_queueMutex);
    }

    void setMaxLen(uint32_t maxLen)
    {
        _maxLen = maxLen;
    }

    bool put(const ElemT& elem, uint32_t maxMsToWait = 0)
    {
        // Get mutex
        if (xSemaphoreTake(_queueMutex, _getMaxTicksToWait(maxMsToWait)) == pdTRUE)
        {
            // Check if queue is full
            if (_queue.size() >= _maxLen)
            {
                // Return semaphore
                xSemaphoreGive(_queueMutex);
                return false;
            }

            // Queue up the item
            _queue.push(elem);

            // Return semaphore
            xSemaphoreGive(_queueMutex);
            return true;
        }
        return false;
    }

    bool get(ElemT& elem, uint32_t maxMsToWait = 0)
    {
        // Get Mutex
        if (xSemaphoreTake(_queueMutex, _getMaxTicksToWait(maxMsToWait)) == pdTRUE)
        {
            if (_queue.empty())
            {
                // Return semaphore
                xSemaphoreGive(_queueMutex);
                return false;
            }

            // read the item and remove
            elem = _queue.front();
            _queue.pop();

            // Return semaphore
            xSemaphoreGive(_queueMutex);
            return true;
        }
        return false;
    }

    bool peek(ElemT& elem, uint32_t maxMsToWait = 0)
    {
        // Get Mutex
        if (xSemaphoreTake(_queueMutex, _getMaxTicksToWait(maxMsToWait)) == pdTRUE)
        {
            if (_queue.empty())
            {
                // Return semaphore
                xSemaphoreGive(_queueMutex);
                return false;
            }

            // read the item (but do not remove)
            elem = _queue.front();

            // Return semaphore
            xSemaphoreGive(_queueMutex);
            return true;
        }
        return false;
    }

    void clear(uint32_t maxMsToWait = 0)
    {
        if (xSemaphoreTake(_queueMutex, _getMaxTicksToWait(maxMsToWait)) == pdTRUE)
        {
            // Clear queue
            while(!_queue.empty()) 
                _queue.pop();

            // Return semaphore
            xSemaphoreGive(_queueMutex);
        }
    }

    uint32_t count(uint32_t maxMsToWait = 0)
    {
        if (xSemaphoreTake(_queueMutex, _getMaxTicksToWait(maxMsToWait)) == pdTRUE)
        {
            int qSize = _queue.size();
            // Return semaphore
            xSemaphoreGive(_queueMutex);
            return qSize;
        }
        return 0;
    }

    uint32_t maxLen()
    {
        return _maxLen;
    }

    bool canAcceptData()
    {
        return _queue.size() < _maxLen;
    }

    void setMaxMsToWait(uint32_t maxMsToWait)
    {
        _maxTicksToWaitDefault = pdMS_TO_TICKS(maxMsToWait);
    }

private:
    std::queue<ElemT> _queue;
    static const uint16_t DEFAULT_MAX_QUEUE_LEN = 50;
    uint16_t _maxLen;
    static const uint16_t DEFAULT_MAX_MS_TO_WAIT = 1;
    uint16_t _maxTicksToWaitDefault;
    inline uint32_t _getMaxTicksToWait(uint32_t maxMsToWait)
    {
        if (maxMsToWait == 0)
            return _maxTicksToWaitDefault;
        return pdMS_TO_TICKS(maxMsToWait);
    }
    // Mutex for queue
    SemaphoreHandle_t _queueMutex;
};
