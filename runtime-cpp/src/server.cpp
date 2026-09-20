#include "scheduler.hpp"
#include "httplib.h"
#include "json.hpp"

#include <csignal>
#include <iostream>
#include <algorithm>

using Json = nlohmann::json;
namespace {
volatile std::sig_atomic_t stopping = 0;
void signal_stop(int) { stopping = 1; }
std::atomic<unsigned long> sequence{0};

void reply(httplib::Response& response, int status, const Json& body) {
    response.status = status;
    response.set_content(body.dump(), "application/json; charset=utf-8");
}
void error(httplib::Response& response, int status, const std::string& id,
           const std::string& code, const std::string& message) {
    reply(response, status, {{"request_id", id}, {"error", {{"code", code}, {"message", message}}}});
}
bool valid_id(const std::string& id) {
    return !id.empty() && id.size() <= 128 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
int number(const char* value, int min, int max) {
    std::size_t used = 0; int n = std::stoi(value, &used);
    if (used != std::string(value).size() || n < min || n > max) throw std::invalid_argument("invalid argument range");
    return n;
}
}

int main(int argc, char** argv) {
    try {
        // runtime_server [port=8082] [workers=2] [capacity=8] [demo_work_ms=0]
        int port = argc > 1 ? number(argv[1], 1, 65535) : 8082;
        int workers = argc > 2 ? number(argv[2], 1, 32) : 2;
        int capacity = argc > 3 ? number(argv[3], 1, 256) : 8;
        int work_ms = argc > 4 ? number(argv[4], 0, 5000) : 0;
        factory::Scheduler scheduler(workers, capacity, work_ms);
        httplib::Server server;
        server.set_payload_max_length(16 * 1024);
        server.set_read_timeout(5, 0);
        server.set_write_timeout(5, 0);
        server.set_keep_alive_max_count(10);
        server.new_task_queue = [] { return new httplib::ThreadPool(16, 32); };

        server.set_pre_routing_handler([](const auto& request, auto& response) {
            auto id = request.get_header_value("X-Request-Id");
            if (!valid_id(id)) id = "cpp-" + std::to_string(++sequence);
            response.set_header("X-Request-Id", id);
            response.set_header("Cache-Control", "no-store");
            return httplib::Server::HandlerResponse::Unhandled;
        });
        server.Get("/health", [&](const auto&, auto& response) {
            reply(response, 200, {{"status", "ok"}, {"backend", "cpp-cpu-demo"}, {"demo_work_ms", work_ms}});
        });
        server.Get("/metrics", [&](const auto&, auto& response) {
            reply(response, 200, {{"queued", scheduler.queued()}, {"active", scheduler.active()},
                {"completed", scheduler.completed()}, {"rejected", scheduler.rejected()}, {"timed_out", scheduler.timed_out()}});
        });
        server.Post("/internal/completions", [&](const httplib::Request& request, httplib::Response& response) {
            const std::string id = response.get_header_value("X-Request-Id");
            auto type = request.get_header_value("Content-Type");
            type = type.substr(0, type.find(';'));
            if (type != "application/json") { error(response, 415, id, "unsupported_media_type", "application/json required"); return; }
            factory::Request input;
            int timeout_ms;
            try {
                auto body = Json::parse(request.body);
                if (!body.is_object() || body.size() != 3 || !body.at("request_id").is_string()
                    || !body.at("prompt").is_string() || !body.at("timeout_ms").is_number_integer())
                    throw std::invalid_argument("expected request_id, prompt, timeout_ms");
                input = {body.at("request_id").get<std::string>(), body.at("prompt").get<std::string>()};
                const auto budget = body.at("timeout_ms").get<std::int64_t>();
                if (budget < 1 || budget > 10000 || !valid_id(input.request_id) || input.request_id != id)
                    throw std::invalid_argument("invalid request_id or timeout_ms");
                timeout_ms = static_cast<int>(budget);
                if (input.prompt.empty() || input.prompt.size() > 8192 || std::all_of(input.prompt.begin(), input.prompt.end(), [](char c) {
                    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
                })) throw std::invalid_argument("prompt must contain 1-8192 bytes and not be ASCII whitespace");
            } catch (const std::exception&) { error(response, 400, id, "invalid_request", "invalid runtime request"); return; }

            auto deadline = factory::Scheduler::Clock::now() + std::chrono::milliseconds(timeout_ms);
            std::future<factory::Result> future;
            if (!scheduler.submit(std::move(input), deadline, future)) {
                response.set_header("Retry-After", "1");
                error(response, 503, id, "queue_full", "runtime queue is full or stopping"); return;
            }
            if (future.wait_until(deadline) != std::future_status::ready) {
                error(response, 504, id, "runtime_timeout", "runtime deadline exceeded"); return;
            }
            auto result = future.get();
            if (result.response.status != "success") {
                int status = result.response.status == "timeout" ? 504 : result.response.status == "unavailable" ? 503 : result.response.status == "invalid_request" ? 400 : 500;
                error(response, status, id, result.response.status, result.response.error); return;
            }
            reply(response, 200, {{"request_id", id}, {"content", result.response.content}, {"backend", "cpp-cpu-demo"},
                {"queue_ms", result.queue_ms}, {"compute_ms", result.compute_ms}, {"demo_work_ms", work_ms}});
        });
        server.set_error_handler([](const auto&, auto& response) {
            if (response.body.empty()) error(response, response.status, response.get_header_value("X-Request-Id"), "http_error", "HTTP request rejected");
        });
        server.set_exception_handler([](const auto&, auto& response, std::exception_ptr) {
            error(response, 500, response.get_header_value("X-Request-Id"), "internal_error", "runtime internal error");
        });
        server.set_logger([](const auto&, const auto& response) {
            static std::mutex log_mutex;
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cout << "request_id=" << response.get_header_value("X-Request-Id") << " status=" << response.status << std::endl;
        });
        if (!server.bind_to_port("127.0.0.1", port)) { std::cerr << "Cannot bind runtime port\n"; return 1; }
        std::signal(SIGINT, signal_stop);
        std::signal(SIGTERM, signal_stop);
        std::atomic<bool> finished{false};
        std::thread shutdown([&] {
            while (!stopping && !finished) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (stopping) { scheduler.stop(); server.stop(); }
        });
        std::cout << "C++ CPU demo runtime on http://127.0.0.1:" << port << std::endl;
        bool success = server.listen_after_bind();
        finished = true;
        shutdown.join();
        return success ? 0 : 1;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
