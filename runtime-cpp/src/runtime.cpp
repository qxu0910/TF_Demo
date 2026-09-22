#include "runtime.hpp"
#include "byte_sum.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace factory {

Response Runtime::handle(const Request& request) const {
    // ASCII 空白检查；UTF-8 中文字节不会被误判成空白。
    const bool blank = std::all_of(request.prompt.begin(), request.prompt.end(), [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
    });
    if (request.request_id.empty() || request.request_id.size() > 128) {
        return {request.request_id, "invalid_request", "", "request_id must contain 1-128 bytes"};
    }
    if (blank || request.prompt.size() > 8192) {
        return {request.request_id, "invalid_request", "", "prompt must contain 1-8192 bytes and not be ASCII whitespace"};
    }

    const auto sample = compute::measure_byte_sum(request.prompt);
    const bool cuda = std::string(compute::backend()) == "cpp-cuda-demo";
    std::string reply = std::string(cuda ? "C++ CUDA demo processed " : "C++ CPU demo processed ")
        + std::to_string(request.prompt.size()) + " bytes. Byte sum = " + std::to_string(sample.sum)
        + ". This is deterministic teaching output, not language model inference. "
        + (cuda ? "CUDA reduction ran on device 0." : "No GPU was called.");

    // Response 拥有字符串，不引用计算后端的临时内存。
    std::ostringstream timing;
    timing << std::fixed << std::setprecision(6) << "\nCompute call total: " << sample.total_ms << " ms.";
    if (sample.kernel_ms) {
        timing << " H2D host wait: " << *sample.h2d_ms << " ms; kernel event interval: "
               << *sample.kernel_ms << " ms; D2H host wait: " << *sample.d2h_ms << " ms.";
    } else timing << " GPU stage timings: N/A.";
    return {request.request_id, "success", reply + timing.str(), "", sample};
}

} // namespace factory
