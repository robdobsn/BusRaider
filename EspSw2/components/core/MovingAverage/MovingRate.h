/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Moving Rate
//
// Rob Dobson 2022
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <ArduinoTime.h>
#include <Utils.h>

/*
 * Moving Rate
 *
 * Calculate the rate of a value like bytes per second
 * The rate is calculated over a moving window of N samples
 * 
 * Template N is the window size
 * Template input_t is the type of the input value
 * Template sum_t is the type of the sum
 * 
 */
template <uint8_t N, class input_t = uint32_t, class sum_t = uint64_t>
class MovingRate
{
public:
    MovingRate()
    {
        clear();
    }

    // sample
    void sample(input_t input) 
    {
        previousInputs[index] = input;
        previousTimeMs[index] = millis();
        if (++index == N)
            index = 0;
        if (usedIdxs < N)
            usedIdxs++;
    }

    // get rate
    double getRatePerSec()
    {
        uint32_t lastIdx = (index + N - 1) % N;
        uint32_t firstIdx = (lastIdx + N + 1 - usedIdxs) % N;
        uint32_t timeDeltaMs = Utils::timeElapsed(previousTimeMs[lastIdx], previousTimeMs[firstIdx]);
        if (timeDeltaMs == 0)
            return 0;
        return (previousInputs[lastIdx] - previousInputs[firstIdx]) * 1000.0 / timeDeltaMs;
    }

    void clear()
    {
        index = 0;
        for (uint8_t i = 0; i < N; i++)
        {
            previousInputs[i] = 0;
            previousTimeMs[i] = 0;
        }
    }

private:
    uint8_t index = 0;
    uint8_t usedIdxs = 0;
    input_t previousInputs[N] = {};
    unsigned long previousTimeMs[N] = {};
};

