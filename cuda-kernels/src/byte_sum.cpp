#include "byte_sum.hpp"
#include <stdexcept>
#include <chrono>

namespace factory::compute {
std::uint32_t byte_sum_cpu(std::string_view input) {
    if (input.size() > 8192) throw std::invalid_argument("byte_sum limit is 8192 bytes");
    std::uint32_t sum = 0;
    for (unsigned char value : input) sum += value;
    return sum;
}
#ifndef FACTORY_CUDA
Sample measure_byte_sum(std::string_view input) {
    const auto start = std::chrono::steady_clock::now();
    Sample result;
    result.sum = byte_sum_cpu(input);
    result.total_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}
std::uint32_t byte_sum(std::string_view input) { return measure_byte_sum(input).sum; }
const char* backend() { return "cpp-cpu-demo"; }
void check_backend() {}
#endif
}
