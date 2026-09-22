#pragma once
#include <cstdint>
#include <string_view>
#include <optional>

namespace factory::compute {
struct Sample {
    std::uint32_t sum = 0;
    double total_ms = 0;
    // CPU 没有设备阶段，用空值表示，不把缺失数据伪装成 0 ms。
    std::optional<double> h2d_ms, kernel_ms, d2h_ms;
};
Sample measure_byte_sum(std::string_view input);
// 输入上限与服务一致；所有字节均按 unsigned char 解释。
std::uint32_t byte_sum_cpu(std::string_view input);
std::uint32_t byte_sum(std::string_view input);
const char* backend();
// CUDA 构建会实际执行小算子；无设备或驱动错误时抛异常。
void check_backend();
}
