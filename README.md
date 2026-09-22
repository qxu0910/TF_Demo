# Mini Token Factory

一个从 Java 逐步下潜到 C++、CUDA 的学习项目。当前主体已实现：

```text
L9 浏览器页面
  → POST /v1/chat/completions
L8 Java 网关：输入校验、请求标识、下游超时与错误映射
  → POST /internal/completions
L7 C++ Runtime：有界队列、工作线程、CPU 字节归约演示
```

这是可运行的 CPU 教学链路，不是真实语言模型。默认路径没有接入权重、Tokenizer、GPU、NCCL；也没有把未来的 12 周学习目标标为完成。

## Windows 启动

需要 Windows JDK 17+、WSL Ubuntu 的 g++ / CMake / make。可在 Ubuntu 中安装 `sudo apt-get install g++ cmake make`。本机已有便携 JDK 时，Java 脚本会自动发现 `tmp/toolchains/jdk`。

终端一，启动 C++：

```powershell
./runtime-cpp/start.ps1
```

终端二，启动 Java：

```powershell
./gateway-java/start.ps1
```

打开 http://127.0.0.1:8081 。C++ 使用 8082。WSL 服务通过 Windows 的 localhost 转发访问。两个服务均默认只监听回环地址；按 Ctrl+C 分别停止。

第一次构建会下载固定版本的 HTTP、JSON 依赖；C++ 依赖校验 SHA-256。下载文件、工具链和构建产物不提交 Git。若 Windows 已有 C++17 编译器，也可使用 CMake 原生构建 `runtime_server.exe`。

Linux 下，在两个终端分别运行 `bash runtime-cpp/start.sh` 和 `bash gateway-java/start.sh`。

## 阅读顺序

1. [C++ 入门程序](runtime-cpp/src/main.cpp)：保留最初的对象与内存练习。
2. [Java 网关](gateway-java/src/main/java/factory/Gateway.java)：入口校验和响应。
3. [下游客户端](gateway-java/src/main/java/factory/RuntimeClient.java)：Java 如何调用 C++。
4. [C++ HTTP 服务](runtime-cpp/src/server.cpp)：解析请求并提交任务。
5. [调度器](runtime-cpp/src/scheduler.cpp)：队列、线程、future、期限和停止。
6. [计算实现](runtime-cpp/src/runtime.cpp)：CPU 字节求和，后续更换为真实模型算子。

详见 [接口契约](docs/interfaces.md)、[启动与排障](docs/runbook.md)。
新增 [计算阶段计时与可重复基准](docs/benchmarks.md)：查看传输、kernel 和总调用耗时，并保存正确性验证后的 JSON 报告。

## 验证

Ubuntu / Linux 的仓库目录中：

```sh
ctest --test-dir runtime-cpp/build --output-on-failure
python3 runtime-cpp/tests/test_http.py
```

主服务启动后，在 Windows 或 Linux 运行：

```sh
python gateway-java/tests/test_gateway.py
python gateway-java/tests/test_failures.py
```

后一个测试会创建隔离的 Java 网关和可控假下游，验证 502/503/504、错误响应和请求标识不匹配，不影响主服务。

## 当前边界

新增 [CUDA 字节归约教学后端](cuda-kernels/README.md)：默认仍为 CPU，显式 ENABLE_CUDA=ON 可构建真实 CUDA 路径；设备验证需在有 NVIDIA GPU 的环境执行。它直接被 Runtime 调用，尚不涉及模型推理。

- 支持单轮、非流式请求。`factory-mock-v1` 是沿用的教学模型标识，不是实际加载的模型。
- Java 默认调用 C++。仅显式指定 `-RuntimeMode mock` 才使用旧 Java Mock，不会故障时自动回退。
- 支持健康检查、就绪检查、请求日志、队列/计算耗时和计数指标。
- 暂未实现真实 Token 计量、鉴权配额、动态批处理、KV Cache 或 Kubernetes 部署。它们是后续学习内容。
- 本地进度和笔记仍存放在浏览器，不随 Git 同步。
