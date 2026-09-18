# Mini Token Factory 前端

第一阶段是 L9 业务应用层，预留下游 L8 Java 网关接口。原生 HTML、CSS、JavaScript，无第三方依赖，无构建步骤。

推荐使用 Java 17 或更高版本，在仓库根目录运行 `java --add-modules jdk.httpserver frontend/serve/FrontendServer.java`，打开 `http://127.0.0.1:8080`。这个传统 Java 服务使用 JDK 内置 HTTP Server，无 Maven、Spring 或其他依赖，按 Ctrl+C 停止。它只负责静态文件，不处理推理请求。

没有 Java 时，可运行 `python -m http.server 8080 --bind 127.0.0.1 --directory frontend` 预览。也可直接打开 `frontend/index.html`；学习记录的持久化取决于浏览器对本地文件存储的支持。

## 代码阅读顺序

1. `index.html`：页面结构、输入框与响应容器。
2. `styles.css`：布局、配色与移动端适配。
3. `app.js`：从 `chat-form` 提交事件开始，跟踪 `mockCompletion` 和 `addMessage`。

## 当前边界

- 对话为预设教学文本，所有请求处理都在浏览器完成，没有后端服务、真实模型或 GPU 调用。
- 请求轨迹含人为等待；展示的是浏览器演示耗时，不是推理性能。请求次数仅统计当前页面会话。
- 路线完成状态和手动保存的笔记存放在当前浏览器 localStorage，不会提交至 GitHub。清除浏览器数据会删除这些记录。
- 清空对话仅清空消息和当前轨迹，不重置当前会话请求次数。

## 验收与下一步

发送预设或自定义问题，确认轨迹依次完成、按钮恢复、重复发送正常；空白输入不发送。学习路线和保存笔记刷新后仍可读取。检查手机宽度无横向滚动。

下一阶段在 L8 实现 `POST /v1/chat/completions`，以真实 HTTP 调用替换 `mockCompletion`，增加后端超时与错误展示。页面交互问题先定位 L9；接入后若 HTTP 返回错误或超时，再检查 L8 网关日志与请求标识。
