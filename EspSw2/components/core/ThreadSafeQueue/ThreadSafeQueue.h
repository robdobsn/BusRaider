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
    ThreadSafeQueue()
    {
        // Mutex for ThreadSafeQueue
        _queueMutex = xSemaphoreCreateMutex();
        _maxLen = DEFAULT_MAX_QUEUE_LEN;
    }

    virtual ~ThreadSafeQueue()
    {
    }

    void setMaxLen(uint32_t maxLen)
    {
        _maxLen = maxLen;
    }

    bool put(const ElemT& elem)
    {
        // Get mutex
        if (xSemaphoreTake(_queueMutex, 1) == pdTRUE)
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

    bool get(ElemT& elem)
    {
        // Get Mutex
        if (xSemaphoreTake(_queueMutex, 1) == pdTRUE)
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

    void clear()
    {
        if (xSemaphoreTake(_queueMutex, 1) == pdTRUE)
        {
            // Clear queue
            while(!_queue.empty()) 
                _queue.pop();

            // Return semaphore
            xSemaphoreGive(_queueMutex);
        }
    }

    uint32_t count()
    {
        if (xSemaphoreTake(_queueMutex, 1) == pdTRUE)
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

private:
    std::queue<ElemT> _queue;
    static const uint16_t DEFAULT_MAX_QUEUE_LEN = 1000;
    uint16_t _maxLen;
    // Mutex for queue
    SemaphoreHandle_t _queueMutex;
};
