package factory;

import com.google.gson.*;
import java.net.URI;
import java.net.http.*;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Map;
import java.util.concurrent.*;

/** L8 → L7 边界：真实 HTTP 调用、总等待期限、请求标识校验。 */
public final class RuntimeClient {
    private static final Gson JSON = new GsonBuilder().setStrictness(Strictness.STRICT).create();
    private final URI base;
    private final int timeoutMs;
    private final HttpClient client = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(1))
        .followRedirects(HttpClient.Redirect.NEVER).build();
    private final boolean mock;
    private volatile String observedBackend = "cpp-unknown";
    private static boolean validBackend(String name) {
        return name.equals("cpp-cpu-demo") || name.equals("cpp-cuda-demo");
    }

    public RuntimeClient() {
        String mode = System.getProperty("factory.runtime.mode", "cpp");
        if (!mode.equals("cpp") && !mode.equals("mock")) throw new IllegalArgumentException("runtime mode must be cpp or mock");
        mock = mode.equals("mock");
        base = URI.create(System.getProperty("factory.runtime.url", "http://127.0.0.1:8082"));
        if (!base.getScheme().equals("http") || base.getHost() == null || base.getUserInfo() != null
            || base.getQuery() != null || base.getFragment() != null || !(base.getPath().isEmpty() || base.getPath().equals("/")))
            throw new IllegalArgumentException("runtime URL must be an HTTP origin");
        timeoutMs = Integer.parseInt(System.getProperty("factory.runtime.timeoutMs", "2000"));
        if (timeoutMs < 50 || timeoutMs > 8000) throw new IllegalArgumentException("timeout must be 50-8000 ms");
    }

    public String mode() { return mock ? "java-mock" : observedBackend; }
    public record Result(String content, double queueMs, double computeMs, int demoWorkMs, String backend, JsonObject profile) {}

    public Result complete(String id, String prompt) {
        if (mock) return new Result(new MockRuntime().complete(prompt), 0, 0, 0, "java-mock", new JsonObject());
        String body = JSON.toJson(Map.of("request_id", id, "prompt", prompt, "timeout_ms", timeoutMs));
        var request = HttpRequest.newBuilder(base.resolve("/internal/completions"))
            .header("Content-Type", "application/json").header("X-Request-Id", id)
            .timeout(Duration.ofMillis(timeoutMs + 250L))
            .POST(HttpRequest.BodyPublishers.ofString(body, StandardCharsets.UTF_8)).build();
        HttpResponse<String> response = exchange(request, timeoutMs + 250);
        if (response.statusCode() == 503) throw new Failure(503, "runtime_busy", "C++ 队列已满或正在停止，请稍后重试");
        if (response.statusCode() == 504) throw new Failure(504, "runtime_timeout", "C++ 请求超过处理期限");
        if (response.statusCode() != 200) throw new Failure(502, "runtime_error", "C++ 返回非预期状态");
        try {
            JsonObject data = JSON.fromJson(response.body(), JsonObject.class);
            if (!id.equals(data.get("request_id").getAsString())
                || !id.equals(response.headers().firstValue("X-Request-Id").orElse(""))
                || !data.get("content").isJsonPrimitive() || !data.get("content").getAsJsonPrimitive().isString()
                || !validBackend(data.get("backend").getAsString())) throw new IllegalArgumentException();
            double queue = data.get("queue_ms").getAsDouble(), compute = data.get("compute_ms").getAsDouble();
            int work = data.get("demo_work_ms").getAsInt();
            if (!Double.isFinite(queue) || !Double.isFinite(compute) || queue < 0 || compute < 0 || work < 0)
                throw new IllegalArgumentException();
            String backend = data.get("backend").getAsString();
            JsonObject profile = validateProfile(data, backend);
            observedBackend = backend;
            return new Result(data.get("content").getAsString(), queue, compute, work, backend, profile);
        } catch (RuntimeException error) { throw new Failure(502, "invalid_runtime_response", "C++ 响应格式或请求标识不正确"); }
    }

    // 旧版本 Runtime 可省略 profile；存在时必须完整且与后端一致。
    private static JsonObject validateProfile(JsonObject data, String backend) {
        if (!data.has("compute_profile")) return new JsonObject();
        JsonObject profile = data.getAsJsonObject("compute_profile");
        if (profile.size() != 5) throw new IllegalArgumentException();
        for (String key : new String[]{"sum", "total_ms", "h2d_host_ms", "kernel_event_ms", "d2h_host_ms"}) {
            JsonElement value = profile.get(key);
            boolean deviceStage = key.endsWith("host_ms") || key.equals("kernel_event_ms");
            if (deviceStage && backend.equals("cpp-cpu-demo")) {
                if (value == null || !value.isJsonNull()) throw new IllegalArgumentException();
                continue;
            }
            if (value == null || !value.isJsonPrimitive() || !value.getAsJsonPrimitive().isNumber())
                throw new IllegalArgumentException();
            double number = value.getAsDouble();
            if (!Double.isFinite(number) || number < 0) throw new IllegalArgumentException();
            if (key.equals("sum") && (number > 2088960 || value.getAsBigDecimal().stripTrailingZeros().scale() > 0))
                throw new IllegalArgumentException();
        }
        return profile;
    }

    public boolean ready() {
        if (mock) return true;
        try {
            var response = exchange(HttpRequest.newBuilder(base.resolve("/health")).timeout(Duration.ofSeconds(1)).GET().build(), 1000);
            var body = JSON.fromJson(response.body(), JsonObject.class);
            boolean ready = response.statusCode() == 200 && body.get("status").getAsString().equals("ok")
                && validBackend(body.get("backend").getAsString());
            if (ready) observedBackend = body.get("backend").getAsString();
            return ready;
        } catch (RuntimeException error) { return false; }
    }

    private HttpResponse<String> exchange(HttpRequest request, int budgetMs) {
        var pending = client.sendAsync(request, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        try {
            var response = pending.get(budgetMs, TimeUnit.MILLISECONDS);
            if (response.body().length() > 65536) throw new Failure(502, "invalid_runtime_response", "C++ 响应过大");
            return response;
        } catch (TimeoutException error) {
            pending.cancel(true);
            throw new Failure(504, "runtime_timeout", "等待 C++ 响应超时");
        } catch (InterruptedException error) {
            pending.cancel(true); Thread.currentThread().interrupt();
            throw new Failure(503, "gateway_stopping", "网关正在停止");
        } catch (ExecutionException error) {
            if (error.getCause() instanceof HttpTimeoutException) throw new Failure(504, "runtime_timeout", "等待 C++ 响应超时");
            throw new Failure(502, "runtime_unavailable", "无法连接 C++ Runtime，请检查服务状态");
        }
    }

    public static final class Failure extends RuntimeException {
        public final int status; public final String code;
        public Failure(int status, String code, String message) { super(message); this.status = status; this.code = code; }
    }
}
