#include "byte_sum.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    try {
        factory::compute::check_backend();
        for (std::size_t n : {0, 1, 255, 256, 257, 511, 512, 8191, 8192}) {
            std::string input(n, '\0');
            std::uint32_t expected = 0;
            for (std::size_t i = 0; i < n; ++i) {
                const auto value = static_cast<unsigned char>((i * 37 + 129) % 256);
                input[i] = static_cast<char>(value);
                expected += value;
            }
            if (factory::compute::byte_sum(input) != expected)
                throw std::runtime_error("sum mismatch at length " + std::to_string(n));
        }
        if (factory::compute::byte_sum(std::string(8192, '\xff')) != 2088960)
            throw std::runtime_error("max sum mismatch");
        bool rejected = false;
        try { factory::compute::byte_sum(std::string(8193, 'x')); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("oversize accepted");
        std::vector<std::future<std::uint32_t>> results;
        for (int i = 0; i < 8; ++i)
            results.push_back(std::async(std::launch::async, [] { return factory::compute::byte_sum("ABC"); }));
        for (auto& result : results) if (result.get() != 198) throw std::runtime_error("concurrent sum mismatch");
        std::cout << factory::compute::backend() << ": byte sum tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
