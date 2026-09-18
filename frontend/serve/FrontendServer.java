import com.sun.net.httpserver.HttpServer;
import java.net.InetSocketAddress;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;

/** Java 17+ 静态页面服务；在仓库根目录运行。仅监听本机。 */
public class FrontendServer {
    public static void main(String[] args) throws Exception {
        Path root = Path.of("frontend").toRealPath();
        Map<String, String> assets = Map.of(
            "/", "text/html; charset=utf-8",
            "/index.html", "text/html; charset=utf-8",
            "/styles.css", "text/css; charset=utf-8",
            "/app.js", "text/javascript; charset=utf-8");
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 8080), 0);
        server.createContext("/", exchange -> {
            try (exchange) {
                String route = exchange.getRequestURI().getPath();
                if (!exchange.getRequestMethod().equals("GET")) {
                    exchange.getResponseHeaders().set("Allow", "GET");
                    exchange.sendResponseHeaders(405, -1);
                    return;
                }
                if (!assets.containsKey(route)) {
                    exchange.sendResponseHeaders(404, -1);
                    return;
                }
                Path file = root.resolve(route.equals("/") ? "index.html" : route.substring(1));
                byte[] body = Files.readAllBytes(file);
                exchange.getResponseHeaders().set("Content-Type", assets.get(route));
                exchange.getResponseHeaders().set("X-Content-Type-Options", "nosniff");
                exchange.sendResponseHeaders(200, body.length);
                exchange.getResponseBody().write(body);
            }
        });
        Runtime.getRuntime().addShutdownHook(new Thread(() -> server.stop(0)));
        server.start();
        System.out.println("Mini Token Factory: http://127.0.0.1:8080 (Ctrl+C to stop)");
    }
}
