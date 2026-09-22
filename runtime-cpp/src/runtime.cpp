#include "runtime.hpp"
#include "byte_sum.hpp"

#include <algorithm>

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

    const auto byte_sum = compute::byte_sum(request.prompt);
    const bool cuda = std::string(compute::backend()) == "cpp-cuda-demo";
    std::string reply = std::string(cuda ? "C++ CUDA demo processed " : "C++ CPU demo processed ")
        + std::to_string(request.prompt.size()) + " bytes. Byte sum = " + std::to_string(byte_sum)
        + ". This is deterministic teaching output, not language model inference. "
        + (cuda ? "CUDA reduction ran on device 0." : "No GPU was called.");

    // Response 拥有字符串，不引用计算后端的临时内存。
    return {request.request_id, "success", reply, ""};
}

} // namespace factory
