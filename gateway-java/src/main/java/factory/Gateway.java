package factory;

import com.google.gson.*;
import com.sun.net.httpserver.*;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.charset.*;
import java.nio.file.*;
import java.time.Instant;
import java.util.*;
import java.util.concurrent.*;

/** L8 网关：路由 → 限制请求体 → 解析与校验 → 调用 L7 → 统一响应。 */
public final class Gateway {
    private static final int MAX_BODY = 16 * 1024;
    private static final Gson JSON = new GsonBuilder().setStrictness(Strictness.STRICT).serializeNulls().create();
    private final RuntimeClient runtime = new RuntimeClient();
    private final java.util.concurrent.atomic.LongAdder completed = new java.util.concurrent.atomic.LongAdder();
    private final java.util.concurrent.atomic.LongAdder failed = new java.util.concurrent.atomic.LongAdder();
    private final Path frontend;

    private Gateway(Path frontend) { this.frontend = frontend; }

    public static void main(String[] args) throws Exception {
        int port = args.length == 0 ? 8081 : Integer.parseInt(args[0]);
        if (port < 1 || port > 65535) throw new IllegalArgumentException("Invalid port");
        Path root = Path.of(System.getProperty("factory.frontend", "frontend")).toRealPath();
        Gateway gateway = new Gateway(root);
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", port), 32);
        // 小型学习服务：固定线程数与有界任务队列，繁忙时由接收线程施加背压。
        ExecutorService pool = new ThreadPoolExecutor(4, 4, 0, TimeUnit.SECONDS,
            new ArrayBlockingQueue<>(32), new ThreadPoolExecutor.CallerRunsPolicy());
        server.setExecutor(pool);
        server.createContext("/", gateway::handle);
        Runtime.getRuntime().addShutdownHook(new Thread(() -> { server.stop(1); pool.shutdownNow(); }));
        server.start();
        System.out.println("Java Gateway (" + gateway.runtime.mode() + "): http://127.0.0.1:" + port);
    }

    private void handle(HttpExchange exchange) throws IOException {
        long started = System.nanoTime();
        String id = "req-" + UUID.randomUUID();
        int status = 500;
        exchange.getResponseHeaders().set("X-Request-Id", id);
        exchange.getResponseHeaders().set("X-Content-Type-Options", "nosniff");
        exchange.getResponseHeaders().set("Cache-Control", "no-store");
        try (exchange) {
            try {
                String path = exchange.getRequestURI().getPath();
                if (path.equals("/health")) {
                    method(exchange, "GET");
                    status = 200;
                    send(exchange, status, Map.of("status", "ok", "runtime", runtime.mode(), "request_id", id));
                } else if (path.equals("/ready")) {
                    method(exchange, "GET");
                    boolean ready = runtime.ready(); status = ready ? 200 : 503;
                    send(exchange, status, Map.of("status", ready ? "ready" : "not_ready", "runtime", runtime.mode(), "request_id", id));
                } else if (path.equals("/metrics")) {
                    method(exchange, "GET"); status = 200;
                    send(exchange, status, Map.of("completed", completed.sum(), "failed", failed.sum(), "runtime", runtime.mode()));
                } else if (path.equals("/v1/chat/completions")) {
                    method(exchange, "POST");
                    String type = exchange.getRequestHeaders().getFirst("Content-Type");
                    if (type == null || !type.split(";", 2)[0].trim().equalsIgnoreCase("application/json"))
                        throw new ApiError(415, "unsupported_media_type", "Content-Type 必须为 application/json");
                    byte[] bytes = exchange.getRequestBody().readNBytes(MAX_BODY + 1);
                    if (bytes.length > MAX_BODY) throw new ApiError(413, "body_too_large", "请求体不能超过 16 KiB");
                    ChatRequest request;
                    try {
                        String text = StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                            .decode(ByteBuffer.wrap(bytes)).toString();
                        request = ChatRequest.parse(JSON.fromJson(text, JsonElement.class));
                    } catch (JsonParseException | IllegalArgumentException | CharacterCodingException error) {
                        throw new ApiError(400, "invalid_request", "JSON 格式或参数不正确：" + error.getMessage());
                    }
                    long runtimeStart = System.nanoTime();
                    RuntimeClient.Result result = runtime.complete(id, request.prompt());
                    double runtimeMs = millisSince(runtimeStart);
                    status = 200;
                    send(exchange, status, Map.of(
                        "id", "chatcmpl-" + id.substring(4), "object", "chat.completion",
                        "created", Instant.now().getEpochSecond(), "model", request.model(), "request_id", id,
                        "choices", List.of(Map.of("index", 0, "message", Map.of("role", "assistant", "content", result.content()), "finish_reason", "stop")),
                        "metadata", Map.of("runtime", result.backend(), "runtime_ms", runtimeMs, "server_ms", millisSince(started),
                            "queue_ms", result.queueMs(), "compute_ms", result.computeMs(), "demo_work_ms", result.demoWorkMs(), "compute_profile", result.profile())));
                    completed.increment();
                } else {
                    Map<String, String> files = Map.of("/", "index.html", "/index.html", "index.html", "/app.js", "app.js", "/styles.css", "styles.css");
                    String filename = files.get(path);
                    if (filename == null) throw new ApiError(404, "not_found", "路径不存在");
                    method(exchange, "GET");
                    byte[] bytes = Files.readAllBytes(frontend.resolve(filename));
                    String type = filename.endsWith(".js") ? "text/javascript" : filename.endsWith(".css") ? "text/css" : "text/html";
                    exchange.getResponseHeaders().set("Content-Type", type + "; charset=utf-8");
                    status = 200;
                    exchange.sendResponseHeaders(status, bytes.length);
                    exchange.getResponseBody().write(bytes);
                }
            } catch (RuntimeClient.Failure error) {
                failed.increment(); status = error.status;
                if (status == 503) exchange.getResponseHeaders().set("Retry-After", "1");
                send(exchange, status, Map.of("request_id", id, "error", Map.of("code", error.code, "message", error.getMessage())));
            } catch (ApiError error) {
                if (exchange.getRequestURI().getPath().equals("/v1/chat/completions")) failed.increment();
                status = error.status;
                send(exchange, status, Map.of("request_id", id, "error", Map.of("code", error.code, "message", error.getMessage())));
            } catch (RuntimeException error) {
                if (exchange.getRequestURI().getPath().equals("/v1/chat/completions")) failed.increment();
                status = 500;
                send(exchange, status, Map.of("request_id", id, "error", Map.of("code", "internal_error", "message", "服务内部错误")));
            }
        } finally {
            // 不记录用户消息正文。标识可与响应头/响应体及浏览器显示关联。
            System.out.printf(Locale.ROOT, "request_id=%s status=%d elapsed_ms=%.3f%n", id, status, millisSince(started));
        }
    }

    private static double millisSince(long start) { return (System.nanoTime() - start) / 1_000_000.0; }
    private static void method(HttpExchange exchange, String expected) {
        if (!exchange.getRequestMethod().equals(expected)) {
            exchange.getResponseHeaders().set("Allow", expected);
            throw new ApiError(405, "method_not_allowed", "请使用 " + expected);
        }
    }
    private static void send(HttpExchange exchange, int status, Object value) throws IOException {
        byte[] bytes = JSON.toJson(value).getBytes(StandardCharsets.UTF_8);
        exchange.getResponseHeaders().set("Content-Type", "application/json; charset=utf-8");
        exchange.sendResponseHeaders(status, bytes.length);
        exchange.getResponseBody().write(bytes);
    }
    private static final class ApiError extends RuntimeException {
        final int status; final String code;
        ApiError(int status, String code, String message) { super(message); this.status = status; this.code = code; }
    }
}
