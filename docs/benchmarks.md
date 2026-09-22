# 字节归约性能测量

主层 L4 驱动与加速运行时，上游 L7 Runtime，下游 NVIDIA 设备与驱动。CPU/CUDA 共用 measure_byte_sum；网页回复和内部响应的测量来自同一次计算。

## 运行基准

仓库根目录执行：

```sh
cmake -S runtime-cpp -B runtime-cpp/build -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=OFF
cmake --build runtime-cpp/build --parallel 2
./runtime-cpp/build/byte_sum_benchmark 8192 100 10
```

参数依次为字节数（1..8192）、测量次数（1..10000）、预热次数（0..1000）。输入固定，每次核对独立的预期值；不匹配则非零退出，不输出成功报告。可重定向标准输出保存 JSON。

CUDA 构建使用独立 build-cuda 目录和 ENABLE_CUDA=ON，设备与架构要求见 [CUDA 教程](../cuda-kernels/README.md)。执行该目录下的 byte_sum_benchmark；没有设备时明确失败，不改跑 CPU。

建议对 1、255、256、257、1024、8192 字节重复实验，使用相同 Release 构建与空闲机器。记录命令、提交号、编译器、GPU 型号、驱动、Toolkit 和原始 JSON。不同机器的结果不能直接组成加速比。

## 指标

| 字段 | 范围 |
|---|---|
| total | 主机单调时钟：整个后端调用，包括显存、流、事件创建与销毁 |
| cpu_reference | 主机单调时钟：同一输入的 CPU 参考求和，不含结果比较 |
| h2d_host | 主机到设备的复制调用及等待完成 |
| kernel_event | 同一 CUDA 流中包围 kernel 的事件区间，不含输出清零 |
| d2h_host | 设备到主机的复制调用及等待完成 |

每项输出 P50/P95（第 50/95 百分位），使用最近秩：排序后取 ceil(p*N) 对应样本，不插值。CPU 设备阶段为 null；CUDA 空输入也没有设备阶段。时钟分辨率可能使极短阶段显示为零。

阶段时钟与覆盖范围不同，阶段之和不等于 total；事件区间也可能受其他 GPU 工作影响。计时本身有开销，不自动宣称加速。CI 运行验证正确性和报告格式，不代表本机性能。

## 流与资源

Stream（执行流：按序提交设备工作的队列）每次调用独立创建。当前在传入、kernel 和传回后分别等待，便于归因，没有实现传输计算重叠。主机字符串使用普通内存，cudaMemcpyAsync 不保证主机完全异步；后续再研究固定页内存、缓冲池与流水线。

Event（事件：记录流中的完成点和设备时间）测量 kernel 区间。异常路径也先等待本次流，再释放显存。HTTP 超时不等于取消 GPU 工作。

## 接口与验收

内部响应 compute_profile 包含 sum、total_ms、h2d_host_ms、kernel_event_ms、d2h_host_ms。Java 校验后透传到 metadata.compute_profile。兼容旧 Runtime：字段缺失时输出空对象，不生成假数据。网页回复附带阶段时间。

工作线程 compute_ms 还包含演示延迟和格式化；compute_profile.total_ms 只覆盖计算调用。

CPU 边界、并发及报告格式由 CI 执行；CUDA CI 只编译。真实 GPU 正确性和性能仍需设备 CTest 与基准验证。设备失败检查 L4 驱动、L1 硬件；算子通过而接口失败回查 L7/L8。

参考：[NVIDIA CUDA 计时最佳实践](https://docs.nvidia.com/cuda/archive/12.8.0/cuda-c-best-practices-guide/)。
