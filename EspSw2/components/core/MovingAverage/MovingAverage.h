/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Moving Average
//
// Rob Dobson 2022
//
// Based on https://tttapa.github.io/Pages/Mathematics/Systems-and-Control-Theory/Digital-filters/Simple%20Moving%20Average/C++Implementation.html
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * Moving Average
 *
 * Calculate the average of a value over a moving window of N samples
 * 
 * Template N is the window size
 * Template input_t is the type of the input value
 * Template sum_t is the type of the sum
 * 
 */

template <uint8_t N, class input_t = uint32_t, class sum_t = uint64_t>
class MovingAverage
{
public:
    MovingAverage()
    {
        clear();
    }
    input_t sample(input_t input) 
    {
        sum -= previousInputs[index];
        sum += input;
        previousInputs[index] = input;
        if (++index == N)
            index = 0;
        return (sum + (N / 2)) / N;
    }

    input_t getAverage() 
    {
        return (sum + (N / 2)) / N;
    }

    void clear()
    {
        index = 0;
        sum = 0;
        for (uint8_t i = 0; i < N; i++)
            previousInputs[i] = 0;
    }

    static_assert(
        // Check that `sum_t` is an unsigned type
        sum_t(0) < sum_t(-1),  
        "Error: sum data type should be an unsigned integer, otherwise, "
        "the rounding operation in the return statement is invalid.");

private:
    uint8_t index             = 0;
    input_t previousInputs[N] = {};
    sum_t sum                 = 0;
};
