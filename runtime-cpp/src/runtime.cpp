#include "runtime.hpp"
#include "buffer.hpp"

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

    // 这里只为观察资源生命周期复制一份输入。生产代码应避免无必要的复制。
    Buffer input(request.prompt);
    std::string reply = "C++ Mock Runtime received " + std::to_string(input.size())
        + " bytes. No model or GPU was called.";

    // 先构造返回对象，再销毁局部变量 input；Response 拥有独立字符串。
    // 没有返回指向 input 内存的指针，因此不会出现悬空指针。
    return {request.request_id, "success", reply, ""};
}

} // namespace factory
