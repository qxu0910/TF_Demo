# L8 / L7 接口契约

## 外部接口：Java

`POST /v1/chat/completions`，Content-Type 为 application/json，UTF-8 请求体最多 16 KiB。

```json
{"model":"factory-mock-v1","messages":[{"role":"user","content":"你好 C++"}],"stream":false}
```

只接受一条 user 消息。content 非空白，最多 2000 个 UTF-16 代码单元；stream 省略或 false。其他字段、错误类型、多轮消息、未知模型均返回 400。

成功格式保留 Chat Completions 的最小字段：id、object、created、model、choices。额外包含 request_id 和 metadata：

```json
{"runtime":"cpp-cpu-demo","queue_ms":0.12,"compute_ms":0.03,"runtime_ms":2.5,"server_ms":3.2,"demo_work_ms":0}
```

上面只是字段示例，不是压测结果。queue_ms 是在 C++ 计算队列里的等待，compute_ms 是工作线程处理耗时（包含显式配置的演示延迟），runtime_ms 是 Java 下游往返，server_ms 截止响应序列化前。不输出伪造的 Token usage。

`X-Request-Id` 由 Java 生成，与响应体、下游请求、两边日志相同。客户端不能指定此标识。

## 内部接口：C++

`POST /internal/completions`，要求 application/json，最多 16 KiB。

请求头：`X-Request-Id: req-example`

```json
{"request_id":"req-example","prompt":"你好 C++","timeout_ms":2000}
```

字段恰好为这三个。request_id 为 1–128 位 ASCII 字母、数字、横线或下划线，且必须与请求头相同。prompt 为非空且非纯 ASCII 空白的 UTF-8 文本，最多 8192 字节；timeout_ms 为 1–10000 的整数。Java 对文本做更严格的前置校验。

成功响应：request_id、content、backend=cpp-cpu-demo、queue_ms、compute_ms、demo_work_ms。C++ 验证后将相对时间预算转换为单调时钟截止时间；排队和演示计算共用这个预算。Java 额外等待 250 ms 网络余量，并通过 future 限制完整响应的等待时间。客户端取消并不保证立即停止服务端；C++ 最迟按自己的期限终止该演示任务。

## 状态与故障

| 情况 | 外部 HTTP 状态 |
|---|---|
| 格式、字段、类型错误 | 400 |
| 未知路径 / 错误方法 | 404 / 405（Java） |
| 请求体过大 / 类型不支持 | 413 / 415 |
| C++ 不可连接、响应格式或请求标识错误 | 502 |
| C++ 计算队列满或服务停止 | 503 |
| C++ 执行期限或 Java 下游等待超时 | 504 |

统一业务错误体：`{"request_id":"…","error":{"code":"…","message":"…"}}`。不透明重试，不伪造成功，不回退到 Mock。

## 健康与指标

- Java `/health`：进程存活；`/ready`：实际探测下游，未就绪返回 503。
- Java `/metrics`：成功/失败的对话请求计数及运行模式。
- C++ `/health`：运行后端和演示延迟；`/metrics`：queued、active、completed、rejected、timed_out。
- 指标是进程内累计计数，重启归零；不是幂等账单。

计算队列有明确容量。HTTP 接入层也限制工作线程和待处理连接；连接层过载可能被断开，Java 将其归为 502，不能把所有网络过载都解释为计算队列 503。

## 可选 CUDA 后端

内部响应 backend 允许 cpp-cpu-demo 或 cpp-cuda-demo；Java metadata.runtime 透传本次响应的 backend。/ready 探测实际后端，尚未探测时 Java 标识为 cpp-unknown。CUDA 构建启动时执行设备自检，不可用时退出，不回退 CPU。compute_ms 包含设备内存分配、数据复制和释放，不能解释为纯 kernel 耗时。
