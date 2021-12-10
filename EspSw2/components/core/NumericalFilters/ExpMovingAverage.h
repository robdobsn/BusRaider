#pragma once

#pragma once
#include <cstdint>     // uint8_t, uint16_t
#include <type_traits> // std::is_unsigned

/// Exponential Moving Average implementation for unsigned integers.
///
/// This code is from https://tttapa.github.io/Pages/Mathematics/Systems-and-Control-Theory/Digital-filters/Exponential%20Moving%20Average/C++Implementation.html#arduino-example

template <uint8_t K, class uint_t = uint16_t>
class ExpMovingAverage {
  public:
    /// Update the filter with the given input and return the filtered output.
    uint_t operator()(uint_t input) {
        state += input;
        uint_t output = (state + half) >> K;
        state -= output;
        return output;
    }

    static_assert(std::is_unsigned<uint_t>::value,
                  "The `uint_t` type should be an unsigned integer, "
                  "otherwise, the division using bit shifts is invalid.");

    /// Fixed point representation of one half, used for rounding.
    constexpr static uint_t half = 1 << (K - 1);

  private:
    uint_t state = 0;
};

