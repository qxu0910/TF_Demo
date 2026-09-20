# 第一节 C++：让一条请求走过对象与内存

本目录保留 L4 运行时基础的 `runtime_demo`，并新增 L7 `runtime_server`。后者已提供 HTTP 接口，默认供 L8 Java 网关调用；GPU 尚未接入。完整启动方式见 [项目说明](../README.md)。初学时仍可以按下面顺序阅读独立 demo。

## 从这里阅读

1. `src/main.cpp`：先只读前三步，看 Request → Runtime → Response。
2. `include/request.hpp`：两个简单数据结构，对照 Java DTO 理解。
3. `src/runtime.cpp`：参数检查、创建缓冲区、构造返回对象。
4. `include/buffer.hpp`：看 vector 如何拥有和释放内存。
5. 回到 main 的第四步，观察拷贝后修改与移动后的结果。

| C++ 写法 | 用 Java 经验理解 |
|---|---|
| `Request request{...}` | 创建局部值对象，离开作用域时销毁 |
| `std::string` | 拥有字符串内存的类型，按值复制内容 |
| `const Request&` | 只读借用现有对象，不复制请求 |
| `std::unique_ptr` | 独占一个堆对象，自动释放；不可普通拷贝 |
| `runtime->handle(...)` | 通过指针调用成员函数 |
| `std::vector<char>` | 自动管理可变长字节数组及其内存 |
| `std::move(x)` | 允许移动资源，本身不是复制或搬运操作 |

RAII（资源获取即初始化）把资源与对象生命周期绑定。Buffer 的 vector 自己管理内存，所以不需要手写 delete 或析构函数。这也避免了复制裸指针造成重复释放。Java 的 GC（垃圾回收）通常不能保证对象在离开作用域时立即被回收。

## 构建与运行

需要 C++17 编译器和 CMake（Unix Makefiles 生成器还需要 make）。Windows 上也可进入 WSL 的 Ubuntu，在仓库对应目录执行以下 Linux 命令。

在仓库根目录运行：

```sh
cmake -S runtime-cpp -B runtime-cpp/build
cmake --build runtime-cpp/build --config Debug
ctest --test-dir runtime-cpp/build -C Debug --output-on-failure
```

Linux/GCC、Ninja 或 Makefile 构建：`./runtime-cpp/build/runtime_demo`。
Windows Visual Studio 构建：`./runtime-cpp/build/Debug/runtime_demo.exe`。
传入自己的问题：`runtime_demo "Hello C++"`。当前命令行只使用第一个参数；UTF-8 输入按字节统计，不代表字符数或 Token 数。

没有 CMake，但已有 g++ 时可直接编译：

```sh
mkdir -p runtime-cpp/build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iruntime-cpp/include runtime-cpp/src/main.cpp runtime-cpp/src/runtime.cpp -o runtime-cpp/build/runtime_demo
./runtime-cpp/build/runtime_demo
```

正常执行会显示请求标识、success、输入字节数，以及 J、L、4 三个拷贝/移动实验结果。空输入 `runtime_demo ""` 返回 invalid_request，退出码为 1。

## 三个动手实验

1. 修改默认 prompt，预测字节数再运行。
2. 将 `copied.set(0, 'L')` 改成别的字符，确认 original 不受影响。
3. 将下标改成 100，观察越界异常如何被 main 捕获，退出码为 2。

验收：正常和错误输入行为可预测；拷贝互不影响；移动后的目标保留数据；测试通过。有 GCC/Clang 的 Linux 环境还可用 AddressSanitizer（地址检查器：检测越界和生命周期错误）进一步验证。

排障先检查本节 L4 的编译与对象行为；服务链路按请求标识分别检查 L8 调用和 L7 服务。接着读 `src/server.cpp` 和 `src/scheduler.cpp`，理解网络接口、有界请求队列和固定线程池。

`runtime_server [port] [workers] [queue_capacity] [demo_work_ms]` 默认参数为 8082、2、8、0。测试见 `tests/test_http.py` 和 `scheduler_tests.cpp`。依赖使用 [cpp-httplib v0.20.0](https://github.com/yhirose/cpp-httplib/tree/v0.20.0) 与 [nlohmann/json v3.11.3](https://github.com/nlohmann/json/tree/v3.11.3)，固定 SHA-256；可设置 CMake 的 RUNTIME_DEPS_DIR 使用已有已校验头文件。
