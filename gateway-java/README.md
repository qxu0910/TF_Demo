# Java 网关

默认链路为浏览器 → Java → C++ CPU 演示后端。采用 Java 17+ 内置 HTTP Server 和 Gson，无 Spring。

从根目录启动：`./gateway-java/start.ps1`（Windows），或 `bash gateway-java/start.sh`（Linux）。页面 http://127.0.0.1:8081，C++ 默认 http://127.0.0.1:8082。

Windows 参数：`-Port 8081 -RuntimeUrl http://127.0.0.1:8082 -TimeoutMs 2000`。只有显式 `-RuntimeMode mock` 才使用旧 Java Mock；不会在 C++ 故障时静默回退。

Linux 可用 RUNTIME_URL、RUNTIME_TIMEOUT_MS、RUNTIME_MODE 环境变量设置同样配置。便携 Windows JDK 可置于 tmp/toolchains/jdk；脚本优先使用 PATH 中的 java/javac。编译产物和 Gson 依赖均处于忽略目录。

阅读顺序：Gateway.handle → ChatRequest.parse → RuntimeClient.complete → C++ 的 server.cpp。响应 request_id 与 X-Request-Id 相同，并传到 C++。

接口、状态码和限制见 [接口契约](../docs/interfaces.md)，启动和故障定位见 [运行说明](../docs/runbook.md)。JSON 库用法参考 [Gson 官方文档](https://github.com/google/gson/blob/main/UserGuide.md)。

运行中执行 `python gateway-java/tests/test_gateway.py` 验证完整链路。`python gateway-java/tests/test_failures.py` 自启隔离 Java 网关和故障下游，验证错误映射。测试旧 Mock 模式时设置 GATEWAY_TEST_MODE=java-mock。

默认仅本机监听，目前没有鉴权、限流、计费、流式输出或真实模型。C++ 队列和期限不等价于租户配额。
