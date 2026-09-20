#include "buffer.hpp"
#include "runtime.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <utility>

int main(int argc, char* argv[]) {
    try {
        // 第一步：构造一个请求。类似 new Request(...)，但这里是局部值对象。
        const factory::Request request{"req-cpp-001", argc > 1 ? argv[1] : "Hello from Java learner"};

        // 第二步：创建运行时。unique_ptr 独占堆对象，离开作用域自动 delete。
        // 此处特意演示智能指针；普通局部对象 factory::Runtime runtime; 也完全可以。
        auto runtime = std::make_unique<factory::Runtime>();

        // 第三步：调用并输出结果。-> 用于通过指针访问对象成员。
        const factory::Response response = runtime->handle(request);
        std::cout << "request_id: " << response.request_id << '\n'
                  << "status: " << response.status << '\n';
        if (response.status != "success") {
            std::cerr << "error: " << response.error << '\n';
            return 1;
        }
        std::cout << "content: " << response.content << "\n\n";

        // 第四步：做一个可观察的拷贝/移动实验。
        factory::Buffer original("Java");
        factory::Buffer copied = original; // 内容复制，两个对象拥有独立内存。
        copied.set(0, 'L');
        std::cout << "Original first byte: " << original.at(0) << '\n'; // J
        std::cout << "Copied first byte: " << copied.at(0) << '\n';     // L

        factory::Buffer moved = std::move(copied); // std::move 允许转移资源。
        std::cout << "Moved buffer size: " << moved.size() << '\n';   // 4
        // 不假设 copied 移动后的长度；它仍可安全销毁或重新赋值。
        std::cout << "Leaving main: local objects release their resources automatically.\n";
        return 0;
    } catch (const std::exception& error) {
        // 例如内存分配失败、越界异常：给出错误并以非零状态退出。
        std::cerr << "Runtime error: " << error.what() << '\n';
        return 2;
    }
}
