# Mini Token Factory 前端

L9 页面已接入 L8 Java 网关，下游为 L7 Java Mock Runtime。前端使用原生 HTML、CSS、JavaScript，无构建步骤。

在仓库根目录运行 `./gateway-java/start.ps1`，打开 `http://127.0.0.1:8081`。详见 [Java 网关文档](../gateway-java/README.md)。需要 JDK 17+，Ctrl+C 停止。

旧 `serve/FrontendServer.java`、Python 静态服务器和直接打开 HTML 仅能预览页面，不能处理新的对话接口。

## 代码阅读顺序

1. `index.html`：页面结构、输入框与响应容器。
2. `styles.css`：布局、配色与移动端适配。
3. `app.js`：从 `chat-form` 提交事件开始，跟踪 `gatewayCompletion` 中的 fetch、服务端 `Gateway.handle` 和 `addMessage`。

## 当前边界

- 对话通过真实 HTTP 调用 Java，回复为服务端预设教学文本，没有真实模型或 GPU 调用。每次只发送当前消息，历史气泡不构成多轮上下文。
- 请求标识来自 Java，显示真实浏览器端到端耗时与服务端处理耗时。收到响应后才确认服务端阶段完成，不是实时事件流。请求次数仅统计当前页面会话成功请求。
- 路线完成状态和手动保存的笔记存放在当前浏览器 localStorage，不会提交至 GitHub。清除浏览器数据会删除这些记录。
- 8080 与 8081 是不同来源，旧预览的笔记不会自动迁移。
- 清空对话仅清空消息和当前轨迹，不重置当前会话请求次数。

## 验收与下一步

发送预设或自定义问题，确认轨迹依次完成、按钮恢复、重复发送正常；空白输入不发送。学习路线和保存笔记刷新后仍可读取。检查手机宽度无横向滚动。

已实现 `POST /v1/chat/completions` 和 10 秒客户端超时；失败后恢复输入供重试。下一阶段替换服务端 Mock。页面交互问题先定位 L9；HTTP 错误或超时按请求标识检查 L8 日志。
