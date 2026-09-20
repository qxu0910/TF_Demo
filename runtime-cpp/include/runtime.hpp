#pragma once

#include "request.hpp"

namespace factory {

class Runtime {
public:
    // const Request&：只读借用，类似传入 Java 对象，但明确禁止修改且不复制。
    // 返回 Response 值：交给编译器做返回值优化，不必写 new。
    Response handle(const Request& request) const;
};

} // namespace factory
