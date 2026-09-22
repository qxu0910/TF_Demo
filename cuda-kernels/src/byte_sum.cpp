#include "byte_sum.hpp"
#include <stdexcept>

namespace factory::compute {
std::uint32_t byte_sum_cpu(std::string_view input) {
    if (input.size() > 8192) throw std::invalid_argument("byte_sum limit is 8192 bytes");
    std::uint32_t sum = 0;
    for (unsigned char value : input) sum += value;
    return sum;
}
#ifndef FACTORY_CUDA
std::uint32_t byte_sum(std::string_view input) { return byte_sum_cpu(input); }
const char* backend() { return "cpp-cpu-demo"; }
void check_backend() {}
#endif
}
