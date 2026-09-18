package factory;

/** L7 接口边界：未来用 C++ HTTP 客户端替换这个教学实现。 */
public final class MockRuntime {
    public String complete(String prompt) {
        String lower = prompt.toLowerCase(java.util.Locale.ROOT);
        if (lower.contains("java") || prompt.contains("网关")) {
            return "这条回复来自 Java 服务端的 Mock Runtime。\n\n"
                + "L8 网关已接收真实 HTTP 请求，完成 JSON 解析、参数校验与请求标识分配，再调用 L7 模拟运行时。\n\n"
                + "本阶段还没有鉴权、限流或 Token 计费。下一步可以沿 Gateway → ChatRequest → MockRuntime 阅读代码。";
        }
        if (lower.contains("gpu") || prompt.contains("链路")) {
            return "当前已跑通：L9 浏览器 → L8 Java 网关 → L7 Java Mock Runtime。\n\n"
                + "后续再把模拟运行时换成 C++ 服务，并接入 CUDA（统一计算设备架构：NVIDIA 的 GPU 并行计算平台）。\n\n"
                + "当前没有调用 GPU，也没有真实模型生成。右侧标识可用于匹配 Java 控制台中的请求日志。";
        }
        return "Java 网关已收到你的问题。\n\n今天可以跟踪三步：\n"
            + "1. 浏览器 fetch 发送 JSON。\n2. 网关校验消息并调用 MockRuntime.complete。\n"
            + "3. 浏览器解析 choices[0].message.content 并显示回复。\n\n这是服务端预设教学回复，不是真实模型推理。";
    }
}
