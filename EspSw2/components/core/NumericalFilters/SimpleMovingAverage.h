#include <stdint.h>

// This code from https://tttapa.github.io/Pages/Mathematics/Systems-and-Control-Theory/Digital-filters/Simple%20Moving%20Average/C++Implementation.html

template <uint16_t N, class input_t = uint16_t, class sum_t = uint32_t>
class SimpleMovingAverage {
  public:
    input_t operator()(input_t input) {
        sum -= previousInputs[index];
        sum += input;
        previousInputs[index] = input;
        if (++index == N)
            index = 0;
// #if (sum_t(0) < sum_t(-1))
//         return (sum + (N / 2)) / N;
// #else
        return sum / N;
// #endif
    }

    input_t cur() {
        return sum / N;
    }

    // static_assert(
    //     sum_t(0) < sum_t(-1),  // Check that `sum_t` is an unsigned type
    //     "Error: sum data type should be an unsigned integer, otherwise, "
    //     "the rounding operation in the return statement is invalid.");

  private:
    uint16_t index             = 0;
    input_t previousInputs[N] = {};
    sum_t sum                 = 0;
};