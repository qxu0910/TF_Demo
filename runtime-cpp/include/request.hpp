#pragma once // 同一个头文件在一次编译中只展开一次。

#include <string>
#include "byte_sum.hpp"

namespace factory {

// 类似 Java DTO。struct 的成员默认 public；string 自己管理字符串内存。
struct Request {
    std::string request_id;
    std::string prompt;
};

struct Response {
    std::string request_id;
    std::string status;       // "success" 或 "invalid_request"
    std::string content;      // 成功时的教学回复
    std::string error;        // 失败时的原因
    compute::Sample compute_profile{};
};

} // namespace factory
