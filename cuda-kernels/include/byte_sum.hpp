#pragma once
#include <cstdint>
#include <string_view>

namespace factory::compute {
// 输入上限与服务一致；所有字节均按 unsigned char 解释。
std::uint32_t byte_sum_cpu(std::string_view input);
std::uint32_t byte_sum(std::string_view input);
const char* backend();
// CUDA 构建会实际执行小算子；无设备或驱动错误时抛异常。
void check_backend();
}
