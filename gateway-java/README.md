# L8 Java 网关

真实链路：`L9 浏览器 fetch → L8 Gateway → ChatRequest → L7 Java MockRuntime → JSON 响应`。

采用传统 Java 17+、JDK 内置 HTTP Server 和 Gson，不依赖 Spring。JSON 库参考 [Gson 官方文档](https://github.com/google/gson/blob/main/UserGuide.md)。

## 启动

从仓库根目录执行 `./gateway-java/start.ps1`，打开 http://127.0.0.1:8081 。可用 `-Port 8082` 换端口，Ctrl+C 停止。

脚本优先使用 PATH 的 java/javac，也支持本项目 `tmp/toolchains/jdk/*/bin` 下的便携 JDK。首次从 Maven Central 下载 Gson 2.14.0 到 `tmp/java-libs`，编译输出在 `gateway-java/target/classes`，这些文件不提交 Git。提供 pom.xml 供 IDE 导入。

Linux/macOS 可执行：

```sh
mvn -f gateway-java/pom.xml package dependency:copy-dependencies
java --add-modules jdk.httpserver -cp 'gateway-java/target/classes:gateway-java/target/dependency/*' factory.Gateway 8081
```

页面和接口由同一进程提供，仅监听本机。旧 8080 静态预览不能处理新对话请求。

## 接口契约

`GET /health`：健康状态。`POST /v1/chat/completions`：要求 Content-Type 为 application/json，UTF-8 请求体不超过 16 KiB。

```json
{"model":"factory-mock-v1","messages":[{"role":"user","content":"Java 网关负责什么？"}],"stream":false}
```

这是 Chat Completions 风格的最小子集。只接受一条 user 消息，content 非空白且不超过 2000 个 UTF-16 字符；model 必须为 factory-mock-v1。stream 可省略或为 false。多轮、流式、max_tokens 等不支持的参数明确返回 400，不静默忽略。

成功响应含 id、object、created、model、choices[0].message.content、request_id，以及 metadata.runtime=mock、server_ms、runtime_ms。不输出伪造的 Token usage。X-Request-Id 响应头、响应体与日志一致。

错误格式：`{"request_id":"req-…","error":{"code":"invalid_request","message":"…"}}`。

| HTTP 状态 | 含义 |
|---|---|
| 400 | JSON 或参数错误 |
| 404 | 路径不存在 |
| 405 | 方法错误，含 Allow 响应头 |
| 413 | 请求体超过限制 |
| 415 | Content-Type 不支持 |
| 500 | 内部异常 |

浏览器等待上限 10 秒，失败后恢复输入与按钮；不自动重试。浏览器取消不等于服务端取消执行。轨迹在收到响应后确认服务端阶段完成，不伪造实时服务端事件。

## 学习与验收

从 Gateway.handle 的路由与请求体，跟踪 ChatRequest.parse 参数校验，再到 MockRuntime.complete 生成教学回复。修改一条回复并重启，观察页面变化；尝试空消息，观察 400。

启动后执行 `python gateway-java/tests/test_gateway.py`，覆盖正常请求、JSON 校验、中文转义、体积限制、路由与 20 个并发请求。环境变量 GATEWAY_TEST_URL 可覆盖测试地址。

主层 L8，上游 L9，下游 L7。验收信号是页面收到 Java 回复，页面标识与后端日志对应。先检查 L9 网络请求及同源地址，再通过标识定位 L8；确认网关成功调用运行时后才下查 L7。

本阶段不含鉴权、限流、Token 计费、后端超时传播、C++ 或 GPU；用于本机学习，后续按计划扩展。
