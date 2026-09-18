# Java 工程师 GPU 底层开发 12 周系统学习计划

> 建议投入：每周 8–10 小时，共 12 周。  
> 学习原则：始终围绕同一个端到端项目推进，所有练习最终必须接入完整链路，避免只会孤立的语法、Kernel 或命令。

## 1. 最终目标

完成计划后，应能从一次大模型请求出发，解释并演示以下完整路径：

```text
客户端
  ↓
[L8] Java API网关：鉴权、限流、请求ID、Token预留
  ↓ HTTP/gRPC
[L7] C++推理服务：队列、批处理、超时、模型执行
  ↓
[L6] 通信与框架：NCCL AllReduce / PyTorch DDP
  ↓
[L4] CUDA运行时：显存、Stream、Kernel、异步复制
  ↓
[L2] GPU互联：PCIe、NVLink、P2P
  ↓
[L1] GPU硬件：SM、HBM、计算与数据搬运
```

最终不是“学完 12 门课”，而是交付一套可演示、可压测、可解释的系统：

**Mini Token Factory（迷你 Token 工厂）**

- Java 网关接收兼容 Chat Completion 的请求。
- C++ 服务负责排队、超时、批处理和调用计算后端。
- CUDA（统一计算设备架构：提供 NVIDIA GPU 并行计算平台和运行时）完成核心算子实验。
- NCCL（NVIDIA 集合通信库：负责单机及跨节点多 GPU 集合通信）完成多 GPU 数据同步。
- Python/PyTorch 用于训练与分布式实验验证。
- 系统输出延迟、吞吐、显存和通信指标。
- 文档能够说明每个模块位于哪一层、依赖谁、如何验收。

## 2. 整体呈现效果

### 2.1 最终演示架构

```text
┌───────────────────────────────────────────────────────────────┐
│ [L9 业务应用层]                                                │
│ CLI客户端 / 测试程序 / 简单聊天页面                              │
└──────────────────────────────┬────────────────────────────────┘
                               │ POST /v1/chat/completions
┌──────────────────────────────▼────────────────────────────────┐
│ [L8 平台与网关层] Java Gateway                                │
│ 鉴权 | Request ID | 限流 | Token预算 | 路由 | 用量记录           │
└──────────────────────────────┬────────────────────────────────┘
                               │ HTTP或gRPC
┌──────────────────────────────▼────────────────────────────────┐
│ [L7 模型与推理服务层] C++ Runtime                              │
│ 有界队列 | 线程池 | 动态批处理 | 超时 | KV Cache模拟 | 流式输出   │
└──────────────────────────────┬────────────────────────────────┘
                               │ 调用计算后端
┌──────────────────────────────▼────────────────────────────────┐
│ [L6 通信与计算框架层]                                         │
│ PyTorch DDP | NCCL AllReduce | Rank管理 | 多GPU同步             │
└──────────────────────────────┬────────────────────────────────┘
                               │ CUDA API
┌──────────────────────────────▼────────────────────────────────┐
│ [L4 驱动与加速运行时层] CUDA Kernels                           │
│ Reduction | MatMul | Softmax | RMSNorm | Attention片段         │
└──────────────────────────────┬────────────────────────────────┘
                               │ PCIe / NVLink / HBM
┌──────────────────────────────▼────────────────────────────────┐
│ [L1-L3 硬件、互联与网络层]                                     │
│ GPU | HBM | NUMA | PCIe | NVLink | HCA | RDMA                 │
└───────────────────────────────────────────────────────────────┘
```

### 2.2 最终仓库结构

```text
mini-token-factory/
├─ gateway-java/           Java网关、鉴权、限流、计量
├─ runtime-cpp/            C++请求队列、线程池、批处理和服务接口
├─ cuda-kernels/           CUDA算子及性能基准
├─ distributed/            NCCL与PyTorch DDP实验
├─ model-runtime/          Attention、KV Cache和采样模拟
├─ deploy/                 Docker、Kubernetes和启动配置
├─ observability/          指标、日志和压测脚本
├─ tests/                  单元、集成、正确性和性能测试
└─ docs/
   ├─ architecture.md      九层架构与请求链路
   ├─ interfaces.md        模块接口和数据结构
   ├─ benchmarks.md        性能结果与分析
   └─ runbook.md           部署、验证、排障和回滚
```

## 3. 九层学习范围

不要求每一层都达到相同编码深度。

| 层级 | 本计划要求 | 掌握深度 |
|---|---|---|
| L1 物理硬件层 | GPU、SM、HBM、HCA、健康指标 | 能解释、能检查 |
| L2 节点互联层 | PCIe、NVLink、NUMA、GPU P2P | 能解释、能测试 |
| L3 网络与存储层 | TCP、RDMA、InfiniBand/RoCE | 能解释数据路径、能基础诊断 |
| L4 驱动与运行时层 | C++、Linux、CUDA、Kernel | 重点编码层 |
| L5 容器与调度层 | Docker、Kubernetes、GPU分配 | 能部署、能定位资源问题 |
| L6 通信与框架层 | NCCL、DDP、AllReduce | 重点编码与实验层 |
| L7 模型与推理服务层 | Attention、KV Cache、批处理 | 重点编码层 |
| L8 平台与网关层 | Java API、限流、计量、路由 | 利用既有Java优势 |
| L9 业务应用层 | 请求调用与验收场景 | 能构造场景和验收 |

## 4. 每周固定节奏

每周都执行同一套闭环：

1. **系统定位（30分钟）**：明确本周主层、关联层和在最终架构中的位置。
2. **原理学习（2小时）**：只学习完成本周代码所需的概念。
3. **编码实现（4小时）**：编写可运行的最小版本。
4. **调试与测量（2小时）**：记录正确性、延迟、吞吐或资源数据。
5. **系统接入（1小时）**：把本周模块接入已有链路。
6. **文档复盘（30分钟）**：更新架构图、接口和问题清单。

每周成果必须回答四个问题：

```text
这段代码位于哪一层？
它调用了谁，又被谁调用？
正确性和性能如何证明？
它出现故障时应该向哪一层继续排查？
```

## 5. 12周详细计划

### 第1周：建立系统骨架，不急着深入

**主层：L8 平台与网关层**  
**关联层：L7 模型与推理服务层**

学习内容：

- 阅读完整九层模型。
- 建立 Git 仓库和目录结构。
- 用 Java 创建 `/v1/chat/completions` 接口。
- 暂时使用 Java Mock Runtime 返回固定 Token。
- 定义 Java 到 C++ 的请求、响应和错误结构。

当周代码：

```text
客户端 → Java Gateway → Mock Runtime → 响应
```

当周产出：

- 第一版架构图。
- API（应用程序编程接口：定义客户端调用服务的方式）契约。
- 一条可调用的端到端假链路。

验收：即使尚无 C++ 和 GPU，完整请求已经能跑通。

### 第2周：C++内存与对象生命周期

**主层：L4 驱动与加速运行时层**  
**关联层：L7 模型与推理服务层**

学习内容：

- 指针、引用、数组、栈和堆。
- RAII（资源获取即初始化：利用对象生命周期自动释放资源）。
- 智能指针、拷贝和移动语义。
- 编译、链接、静态库与动态库。
- CMake工程结构。

当周代码：

- `Request`、`Response`和`Buffer`类。
- 使用RAII管理动态缓冲区。
- 使用CMake构建`runtime-cpp`。

系统接入：Java仍调用Mock，但C++ Runtime可以独立执行同一份请求样例。

验收：无内存泄漏，错误输入不会导致崩溃。

### 第3周：Linux并发与网络服务

**主层：L3 网络层、L4 运行时层**  
**关联层：L7 推理服务层**

学习内容：

- 进程、线程、系统调用和信号。
- `std::thread`、互斥锁、条件变量和原子变量。
- TCP（传输控制协议：提供可靠网络连接）Socket。
- 阻塞、非阻塞和epoll。
- gdb、strace和基础perf。

当周代码：

- 有界请求队列。
- 固定线程池。
- C++ HTTP/TCP Runtime。
- 队列满、超时和优雅关闭。

系统接入：

```text
Java Gateway → C++ Runtime → 模拟计算 → Java返回
```

验收：并发请求无死锁，队列满时行为可预期，进程可优雅退出。

### 第4周：CUDA执行模型与第一批Kernel

**主层：L4 驱动与加速运行时层**  
**关联层：L1 硬件层**

学习内容：

- Host、Device、Grid、Block、Thread、Warp和SM。
- GPU显存申请与数据复制。
- CUDA Kernel启动。
- Global Memory和Shared Memory。
- CUDA Event计时。

当周代码：

- Vector Add。
- Reduction求和。
- CPU版本与GPU版本正确性对比。
- 不同数据规模的耗时记录。

系统接入：C++ Runtime增加`/compute/reduce`内部调用，真正执行CUDA代码。

验收：结果一致，并能区分内存复制时间和Kernel执行时间。

### 第5周：矩阵计算、异步执行与内存优化

**主层：L4 驱动与加速运行时层**  
**关联层：L7 推理服务层**

学习内容：

- 矩阵乘法与内存访问合并。
- Shared Memory分块。
- Pinned Memory。
- CUDA Stream和异步数据复制。
- Softmax和RMSNorm基本结构。

当周代码：

- 朴素矩阵乘法。
- 分块矩阵乘法。
- Softmax或RMSNorm算子。
- Stream并发实验。

系统接入：C++ Runtime把模拟计算替换成矩阵计算流水线。

验收：每次优化都有基线、正确性比较和性能数据，不能只写“更快”。

### 第6周：性能分析而不是盲目优化

**主层：L4 驱动与加速运行时层**  
**关联层：L1 硬件层、L7 推理服务层**

学习内容：

- CPU/GPU时间线。
- Kernel占用率和内存带宽。
- 同步等待和空闲区间。
- Nsight Systems与Nsight Compute。
- 性能瓶颈分类：计算受限、内存受限、传输受限。

当周代码与产出：

- 为关键阶段增加性能标记。
- 采集一次完整请求的CPU/GPU时间线。
- 输出一份性能分析报告。
- 只优化报告证明的首要瓶颈。

验收：能够用时间线说明请求慢在网关、排队、复制、Kernel还是同步。

### 第7周：GPU拓扑与多GPU基础

**主层：L2 节点互联层**  
**关联层：L4 运行时层、L6 通信层**

学习内容：

- PCIe（高速串行计算机扩展总线：连接CPU、GPU和HCA等设备）。
- NVLink（NVIDIA GPU高速互联技术：提供GPU间高带宽连接）。
- NUMA（非一致性内存访问：描述CPU、内存和PCIe设备亲和关系）。
- P2P（点对点访问：允许GPU直接访问另一张GPU显存）。
- `nvidia-smi topo -m`输出解读。

当周代码：

- 查询GPU数量和属性。
- 检查GPU P2P能力。
- 两GPU显存复制实验。
- 比较不同GPU对之间的传输性能。

系统接入：Runtime启动时输出GPU拓扑摘要并选择目标GPU。

验收：能解释为什么不同GPU组合的通信速度可能不同。

### 第8周：NCCL与分布式训练

**主层：L6 通信与计算框架层**  
**关联层：L2 节点互联层、L3 集群网络层**

学习内容：

- Rank（分布式进程编号：标识通信组中的唯一进程）。
- World Size（全局进程总数：参与任务的全部进程数量）。
- Communicator、AllReduce、AllGather和ReduceScatter。
- PyTorch DDP（分布式数据并行：让各GPU处理不同数据并同步梯度）。
- 单进程多GPU与多进程多GPU。

当周代码：

- C++ NCCL两GPU AllReduce。
- PyTorch两GPU DDP最小训练程序。
- 对比单GPU与双GPU吞吐和同步开销。

系统接入：C++ Runtime增加可选的多GPU计算模式。

验收：所有Rank结果一致；异常Rank能被识别；通信耗时有记录。

### 第9周：Transformer推理主链路

**主层：L7 模型与推理服务层**  
**关联层：L4 运行时层、L6 框架层**

学习内容：

- Tokenizer和Embedding。
- RMSNorm、RoPE、Attention、MLP和采样。
- Prefill与Decode的区别。
- 模型权重和Tensor内存布局。

当周代码：

- 简化Attention实现。
- CPU参考版本与CUDA版本对比。
- 实现最简单的Token采样。
- 阅读`llama.cpp`的一条推理调用链。

系统接入：Runtime从“矩阵演示服务”升级为“简化推理流水线”。

验收：能从输入Token解释到输出Token经历的主要算子和数据流。

### 第10周：KV Cache、批处理和量化

**主层：L7 模型与推理服务层**  
**关联层：L4 运行时层、L8 网关层**

学习内容：

- KV Cache（键值缓存：复用注意力历史状态以避免重复计算）。
- 连续批处理和请求调度。
- Cache Block、淘汰和容量估算。
- FP16、INT8、INT4基本量化思想。
- TTFT与TPOT。

其中：

- TTFT（首Token延迟：请求发出到首个输出Token返回的时间）。
- TPOT（单个输出Token平均耗时：衡量解码阶段生成速度）。

当周代码：

- 简化KV Cache管理器。
- 按Token预算进行准入控制。
- 多请求批处理模拟。
- 记录不同并发下的TTFT、TPOT和显存占用。

系统接入：Java网关把`max_tokens`和请求优先级传给C++ Runtime。

验收：缓存不足时系统排队或拒绝请求，而不是运行中途随机崩溃。

### 第11周：Java与C++服务完整集成

**主层：L8 平台与网关层**  
**关联层：L7 推理服务层**

学习内容：

- HTTP或gRPC服务边界。
- 请求ID、超时传播和错误码。
- RPM（每分钟请求数：用于请求频率限流）。
- TPM（每分钟Token数：用于大模型Token配额）。
- Token预留与实际结算。
- 幂等计量。

当周代码：

- Java网关鉴权与限流。
- 请求进入前预估Token。
- C++ Runtime返回实际用量。
- Java记录计量事件。
- 首Token前失败允许重试，首Token后失败不透明重试。

验收：同一请求不会重复计量；超时和错误能跨Java/C++边界正确传播。

### 第12周：部署、压测和系统答辩

**主层：L5 容器与调度层**  
**关联层：L7 推理服务层、L8 平台层**

学习内容：

- Docker镜像。
- Kubernetes资源配置。
- GPU Device Plugin资源申请。
- 健康检查、日志和指标。
- 压测、故障注入和Runbook。

当周任务：

```text
[L5] 容器化Java网关和C++ Runtime
[L5] 配置GPU资源和健康检查
[L7] 执行单请求、并发和长上下文测试
[L6] 执行多GPU通信测试
[L8] 验证限流、超时和Token计量
[L9] 完成一条业务请求端到端演示
```

最终验收：

- 一条请求可以经过Java、C++和CUDA完整返回。
- 可以展示各阶段延迟和GPU资源数据。
- 多GPU模式下可以解释NCCL通信行为。
- 能根据故障现象定位到九层模型中的具体层。
- 文档包含部署、验证、排障和回滚步骤。

## 6. 系统性防碎片机制

### 6.1 每周必须更新架构图

新增模块后，在架构图上标注：

- 模块属于哪一层。
- 接收什么输入。
- 输出什么结果。
- 同步还是异步调用。
- 失败如何向上返回。

### 6.2 每周必须做一次端到端回归

即使本周只学习CUDA Kernel，也要运行：

```text
客户端 → Java网关 → C++服务 → CUDA Kernel → 返回结果
```

禁止只运行孤立的`.cu`文件就宣布完成。

### 6.3 每周必须保留性能基线

建议统一记录：

| 指标 | 说明 |
|---|---|
| 请求总延迟 | 客户端看到的端到端时间 |
| 网关处理时间 | Java鉴权、限流和路由耗时 |
| 队列等待时间 | 请求进入C++队列后的等待时间 |
| CPU处理时间 | C++调度和数据准备耗时 |
| 数据复制时间 | CPU与GPU间数据传输时间 |
| Kernel时间 | GPU计算时间 |
| 通信时间 | NCCL集合通信时间 |
| TTFT | 首Token返回时间 |
| TPOT | 平均输出Token时间 |
| GPU显存 | 权重、临时缓冲和KV Cache占用 |

### 6.4 每个模块必须有上下游契约

例如Java网关和C++ Runtime之间必须明确：

```json
{
  "request_id": "req-001",
  "prompt_tokens": 128,
  "max_tokens": 64,
  "priority": 5,
  "timeout_ms": 30000
}
```

返回结果至少包含：

```json
{
  "request_id": "req-001",
  "completion_tokens": 42,
  "queue_ms": 3,
  "compute_ms": 18,
  "status": "success"
}
```

## 7. 环境要求

### 最低环境

- Linux开发环境；前3周没有GPU也可以完成。
- C++17或更高版本。
- CMake、gdb、Git。
- Java 17或更高版本。
- Python及PyTorch实验环境。

### GPU阶段

- 第4–6周：一张NVIDIA GPU即可。
- 第7–8周：建议至少两张GPU，用于P2P和NCCL实验。
- 没有多GPU环境时，可先完成代码和单GPU验证，待进入实验集群再补多GPU数据。
- 生产集群上执行压力测试前必须获得授权，不将学习实验直接用于生产节点。

## 8. 阶段验收门槛

| 阶段 | 周次 | 必须达到的结果 |
|---|---:|---|
| 系统骨架 | 1 | Java端到端Mock链路可运行 |
| C++与Linux | 2–3 | Java能调用C++服务，并通过并发测试 |
| CUDA计算 | 4–6 | 请求能触发GPU计算并输出性能分析 |
| 多GPU通信 | 7–8 | 能运行和解释AllReduce/DDP实验 |
| 推理核心 | 9–10 | 能解释Attention、KV Cache和批处理代码 |
| 平台集成 | 11 | 网关、Runtime和计量链路连通 |
| 最终交付 | 12 | 完整演示、压测报告和Runbook通过评审 |

如果某阶段没有达到门槛，不应仅通过继续看下一阶段资料来“追进度”，而应先修复代码、测试或理解缺口。

## 9. 最终答辩题目

完成计划后，应能够不看资料回答：

1. 一个请求从Java进入后，数据如何到达GPU？
2. CPU内存、GPU显存和KV Cache分别保存什么？
3. CUDA Stream为什么能够实现计算和传输重叠？
4. AllReduce发生在哪一层，底层可能经过哪些链路？
5. 单GPU正常、多GPU很慢时应该从哪几层排查？
6. 为什么提高Batch可能增加吞吐却恶化TTFT？
7. KV Cache不足时为什么应该在请求进入前进行准入控制？
8. 为什么首Token返回后不能随意进行透明重试？
9. Java、C++、CUDA和Python在这套系统中分别承担什么职责？
10. 如何证明一次优化是真的优化，而不是改变了测试条件？

## 10. 推荐官方资料顺序

1. [NVIDIA CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-programming-guide/)：先读编程模型、CUDA C++和异步执行，再查高级特性。
2. [CUDA C++ Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/contents.html)：用于正确性、性能分析和优化方法。
3. [NVIDIA NCCL Documentation](https://docs.nvidia.com/deeplearning/nccl/)：学习通信器、Rank和集合通信语义。
4. [PyTorch Distributed Overview](https://docs.pytorch.org/tutorials/beginner/dist_overview.html)：建立DDP、FSDP和模型并行的整体视野。
5. [PyTorch DDP Tutorial](https://docs.pytorch.org/tutorials/intermediate/ddp_tutorial.html)：完成最小分布式训练实验。
6. [NVIDIA Nsight Systems](https://docs.nvidia.com/nsight-systems/UserGuide/)：分析CPU、CUDA和GPU执行时间线。
7. [llama.cpp](https://github.com/ggml-org/llama.cpp)：按模型加载、计算图、KV Cache、采样和服务入口逐层阅读。

## 11. 层级总结

- **主攻层**：L4驱动与加速运行时层、L6通信与计算框架层、L7模型与推理服务层。
- **优势复用层**：L8平台与网关层，继续使用Java完成鉴权、路由、限流和计量。
- **上游依赖**：L1硬件、L2互联、L3网络决定GPU计算和通信的性能上限。
- **部署承载层**：L5负责容器化、GPU分配和工作负载运行。
- **最终验收层**：L9通过真实业务请求验证整个系统，而不是孤立验证单个模块。
- **升级边界**：Kernel错误从L7下沉到L4；多GPU性能问题从L6检查L2/L3；资源未分配从L7转到L5；请求侧错误从L9依次检查L8和L7。
