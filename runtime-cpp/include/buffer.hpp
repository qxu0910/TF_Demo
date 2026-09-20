#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace factory {

// CPU 字节缓冲区，不是 GPU 显存。
// RAII（资源获取即初始化）：成员 vector 在构造时获取内存，在销毁时释放。
// Java 通常依靠 GC；这里离开作用域就会调用析构函数，不等待垃圾回收。
class Buffer {
public:
    explicit Buffer(std::string_view text) : bytes_(text.begin(), text.end()) {}

    // const 表示这个成员函数不会修改 Buffer。
    std::size_t size() const { return bytes_.size(); }

    // at() 检查边界，越界会抛 std::out_of_range，不会读写非法内存。
    char at(std::size_t index) const { return bytes_.at(index); }
    void set(std::size_t index, char value) { bytes_.at(index) = value; }

    // 没有手写析构、拷贝或移动函数：交给 vector 的正确实现。
    // 拷贝会复制内容；移动可以转移底层资源。后面在 main.cpp 中观察。
private:
    std::vector<char> bytes_;
};

} // namespace factory
