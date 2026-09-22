# 第一段 CUDA：字节归约

主层 L4 驱动与加速运行时层，上游 L7 C++ Runtime，下游 L1 NVIDIA GPU。
CUDA（统一计算设备架构）提供 GPU 编程与运行接口。本节把原来的 CPU 字节求和抽成计算后端，直接接入已有 HTTP 请求链路。

## 阅读顺序

1. `include/byte_sum.hpp`：C++ 调用接口，无需包含 CUDA 头文件。
2. `src/byte_sum.cpp`：CPU 参考实现，默认构建无需 CUDA。
3. `src/byte_sum.cu`：显存生命周期、复制、kernel（GPU 上执行的函数）、归约。
4. `tests/byte_sum_tests.cpp`：尾块、最大值、空输入、越界和并发验证。
5. `src/benchmark.cpp`：预热、正确性核验与分位数报告，详见 [性能测量](../docs/benchmarks.md)。

每个 block（线程块）有 256 个线程。各线程读取一个字节，不足一块的尾部填零。共享内存中的 256 个值依次合并成 128、64、32……1 个值。所有线程都必须经过每个同步点，不能让越界线程提前 return。最后每块的第一个线程用 atomicAdd 把块结果累加到同一个输出。

调用路径：分配显存 → 主机到设备复制 → 清零输出 → 启动 kernel → 设备到主机复制 → 释放显存。DeviceBuffer 禁止复制，析构时释放资源；每个请求使用独立显存。所有可报告的 CUDA 操作错误抛给服务层，析构中的释放不抛异常。

## CPU 构建

在仓库根目录：

```sh
cmake -S runtime-cpp -B runtime-cpp/build -DENABLE_CUDA=OFF
cmake --build runtime-cpp/build --parallel 2
ctest --test-dir runtime-cpp/build --output-on-failure
```

## CUDA 构建与接入

需要 NVIDIA GPU、兼容驱动、CUDA Toolkit 的 nvcc 和 CMake。使用单独的构建目录。以下 75 是架构示例，必须按实际 GPU 与 Toolkit 支持情况调整 CMAKE_CUDA_ARCHITECTURES。

```sh
cmake -S runtime-cpp -B runtime-cpp/build-cuda -DENABLE_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=75
cmake --build runtime-cpp/build-cuda --parallel 2
ctest --test-dir runtime-cpp/build-cuda --output-on-failure
./runtime-cpp/build-cuda/runtime_server 8082
```

Windows 可在 WSL 中运行这些命令，再正常启动 Windows Java 网关。启动前先停止占用 8082 的旧实例。CUDA 服务启动时执行 ABC=198 自检，失败就退出，不自动回退 CPU。默认 start.ps1/start.sh 仍用于 CPU；CUDA 使用上述显式命令。

Java 通过 /health 和响应中的 backend 识别 cpp-cpu-demo / cpp-cuda-demo；无需修改请求格式，页面显示实际后端。每条响应的 metadata.runtime 来自该请求的返回值，不依赖其他并发请求。

## 验收与限制

- CPU 路径及端到端链路由常规 CI 验证。
- CUDA CI 只编译，不执行 GPU 测试。必须在真实 GPU 上运行 CTest，才能声明设备正确性通过。
- 测试包含长度 255/256/257、8192 个 255 字节以及并发独立输出。最大和 2088960，不会溢出 32 位无符号整数。
- compute_ms 包含显存分配、复制、计算和释放；不是纯 kernel 耗时。输入最多 8 KiB，GPU 开销可能大于 CPU，当前没有加速比结论。
- HTTP 超时不意味着 GPU kernel 被取消。当前同步复制等待完成后才释放显存；未来长任务需要独立的取消与资源回收设计。
- 启动探针验证设备可用性，运行中的 /health 仍是进程存活检查，不持续检测 GPU 健康；计算故障由请求错误暴露。
- 无 nvcc 属于 L4 工具链问题；驱动或设备自检失败先检查 L4/L1；算子成功但 HTTP 失败再检查 L7/L8。

参考：[NVIDIA CUDA 编程指南](https://docs.nvidia.com/cuda/archive/12.9.1/cuda-c-programming-guide/index.html)。
