#include "buffer.hpp"
#include "runtime.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message); // Release 构建也执行检查。
}

int main() {
    try {
        factory::Runtime runtime;
        auto result = runtime.handle({"req-test", "Hello"});
        require(result.status == "success" && result.request_id == "req-test", "response correlation");
        require(result.content.find("5 bytes") != std::string::npos, "input byte count");
        require(runtime.handle({"", "hello"}).status == "invalid_request", "missing request id");
        require(runtime.handle({"r", ""}).status == "invalid_request", "empty prompt");
        require(runtime.handle({"r", " \n\t"}).status == "invalid_request", "blank prompt");
        require(runtime.handle({"r", std::string(8193, 'x')}).status == "invalid_request", "oversized prompt");
        require(runtime.handle({"r", std::string(8192, 'x')}).status == "success", "exact limit");
        require(runtime.handle({"r", u8"你好"}).content.find("6 bytes") != std::string::npos, "UTF-8 bytes");

        factory::Buffer original("abc");
        auto copy = original;
        copy.set(0, 'z');
        require(original.at(0) == 'a', "copy owns independent memory");
        auto moved = std::move(copy);
        require(moved.size() == 3 && moved.at(0) == 'z', "move preserves content");
        bool caught = false;
        try { moved.set(3, 'x'); } catch (const std::out_of_range&) { caught = true; }
        require(caught, "bounds checked write");
        factory::Buffer empty("");
        require(empty.size() == 0, "empty buffer");
        std::cout << "All runtime tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
