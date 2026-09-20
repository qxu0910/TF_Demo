# 运行、验证与排障

主体层：L7 C++ 服务。上游 L8 Java，下游当前是 CPU 教学计算。L4 的编译与内存练习保留在 runtime_demo 中。

## 配置

Java：`./gateway-java/start.ps1 -Port 8081 -RuntimeUrl http://127.0.0.1:8082 -TimeoutMs 2000`。

C++：`./runtime-cpp/start.ps1 -Port 8082 -Workers 2 -Capacity 8 -DemoWorkMs 0`。Linux 对应 `bash runtime-cpp/start.sh 8082 2 8 0`。演示延迟只由启动参数指定，不能由外部请求开启。

为了观察排队，可设置 `-Workers 1 -Capacity 1 -DemoWorkMs 150` 并并发发送请求。演示延迟不是性能基线。

服务默认只监听 127.0.0.1，不应用作公网模型服务。不要同时启动两个占用相同端口的实例。

## 验收顺序

1. C++ 编译和 CTest 通过：对象生命周期、边界检查、队列容量、期限和停止行为。
2. HTTP 隔离测试通过：正常输入、非法输入、队列满、超时、带任务退出。
3. 启动 C++ 和 Java，确认 `/health`、`/ready` 返回 200。
4. 页面发送消息，回复包含 C++ CPU demo；复制请求标识，在两边日志找到同一标识。
5. 停止 C++，Java `/health` 仍为 200，`/ready` 应为 503，对话请求为 502；重启 C++ 后应恢复。
6. Java 故障夹具测试通过：非预期状态、错误标识、格式错误、慢响应和断连均不能作为成功返回。

## 故障定位

| 现象 | 首先检查 |
|---|---|
| 页面打不开 | L8 是否启动、端口是否为 8081 |
| 网关在线，下游未就绪 | L7 8082 进程、RuntimeUrl、WSL localhost 转发 |
| 502 | 请求标识对应日志；下游连接或响应契约 |
| 503 | C++ queued/active/rejected，是否队列满或正在停止 |
| 504 | 排队时间、DemoWorkMs、Java TimeoutMs |
| 编译失败 | L4 编译器 C++17、CMake/make、依赖下载和哈希校验 |

## 停止与回退

C++ 的 SIGINT/SIGTERM 只设置停止标志，后台线程停止接单并释放排队任务；活动演示任务也会结束，工作线程随后 join。HTTP 连接读写超时为 5 秒，退出可能短暂等待已有连接。

Java 用 Ctrl+C 停止 HTTP 服务和线程池。开发时只停止自己启动的进程，不按名字批量杀进程。

要回到 Java 教学 Mock，可显式执行 `./gateway-java/start.ps1 -RuntimeMode mock`。这是一种调试配置，不改写 Git 历史；恢复 C++ 只需改回默认启动方式。浏览器会显示真实模式。

## 已知限制

尚无真实模型、GPU、流式输出、Token 配额、鉴权、动态批处理或分布式通信。C++ HTTP/JSON 依赖版本已固定；升级时需更新校验值并重跑测试。阶段耗时不可直接外推为模型吞吐。
