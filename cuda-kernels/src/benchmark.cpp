#include "byte_sum.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int argument(const char* text, int low, int high) {
    const std::string value(text);
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("arguments must be decimal integers");
    const auto number = std::stoll(value);
    if (number < low || number > high) throw std::invalid_argument("argument outside range");
    return static_cast<int>(number);
}
double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    return values[static_cast<std::size_t>(std::ceil(fraction * values.size())) - 1];
}
void metric(const char* name, const std::vector<double>& values) {
    std::cout << ",\"" << name << "\":";
    if (values.empty()) std::cout << "null";
    else std::cout << "{\"p50_ms\":" << percentile(values, .5)
                   << ",\"p95_ms\":" << percentile(values, .95) << '}';
}
}

int main(int argc, char** argv) {
    try {
        if (argc > 4) throw std::invalid_argument("usage: byte_sum_benchmark [bytes 1..8192] [iterations 1..10000] [warmup 0..1000]");
        const int bytes = argc > 1 ? argument(argv[1], 1, 8192) : 8192;
        const int iterations = argc > 2 ? argument(argv[2], 1, 10000) : 100;
        const int warmup = argc > 3 ? argument(argv[3], 0, 1000) : 10;
        std::string input(bytes, '\0');
        std::uint32_t expected = 0;
        for (int i = 0; i < bytes; ++i) {
            const auto value = static_cast<unsigned char>((i * 37 + 129) % 256);
            input[i] = static_cast<char>(value);
            expected += value; // 独立期望值，不使用被测函数生成答案。
        }
        factory::compute::check_backend();
        for (int i = 0; i < warmup; ++i)
            if (factory::compute::byte_sum(input) != expected) throw std::runtime_error("warmup mismatch");
        std::vector<double> total, h2d, kernel, d2h, cpu;
        for (int i = 0; i < iterations; ++i) {
            const auto start = std::chrono::steady_clock::now();
            const auto reference = factory::compute::byte_sum_cpu(input);
            cpu.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
            const auto sample = factory::compute::measure_byte_sum(input);
            if (reference != expected || sample.sum != expected) throw std::runtime_error("result mismatch");
            total.push_back(sample.total_ms);
            if (sample.h2d_ms) h2d.push_back(*sample.h2d_ms);
            if (sample.kernel_ms) kernel.push_back(*sample.kernel_ms);
            if (sample.d2h_ms) d2h.push_back(*sample.d2h_ms);
        }
        // 输出仅在全部结果正确后生成，便于重定向保存，失败时不会输出成功报告。
        std::cout << std::fixed << std::setprecision(6)
                  << "{\"backend\":\"" << factory::compute::backend()
                  << "\",\"bytes\":" << bytes << ",\"iterations\":" << iterations
                  << ",\"warmup\":" << warmup << ",\"sum\":" << expected
                  << ",\"correct\":true";
        metric("total", total); metric("cpu_reference", cpu);
        metric("h2d_host", h2d); metric("kernel_event", kernel); metric("d2h_host", d2h);
        std::cout << "}\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
