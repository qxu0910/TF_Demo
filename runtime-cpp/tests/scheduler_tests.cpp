#include "scheduler.hpp"
#include <iostream>
#include <stdexcept>

using namespace std::chrono_literals;
using Clock = factory::Scheduler::Clock;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main() {
    try {
        factory::Scheduler scheduler(1, 1, 100);
        std::future<factory::Result> first, queued, refused;
        check(scheduler.submit({"first", "hello"}, Clock::now() + 2s, first), "first accepted");
        auto until = Clock::now() + 1s;
        while (!scheduler.active() && Clock::now() < until) std::this_thread::yield();
        check(scheduler.active() == 1, "worker started");
        check(scheduler.submit({"queued", "hello"}, Clock::now() + 2s, queued), "queued accepted");
        check(!scheduler.submit({"full", "hello"}, Clock::now() + 2s, refused), "queue bound");
        check(first.get().response.status == "success", "first completed");
        auto second = queued.get();
        check(second.response.status == "success" && second.queue_ms > 0, "queued result and timing");
        std::future<factory::Result> expired;
        check(scheduler.submit({"late", "hello"}, Clock::now() + 5ms, expired), "timeout accepted");
        check(expired.get().response.status == "timeout", "deadline enforced");
        check(scheduler.completed() == 2 && scheduler.rejected() == 1 && scheduler.timed_out() == 1, "metrics");

        std::future<factory::Result> running, pending;
        check(scheduler.submit({"running", "hello"}, Clock::now() + 2s, running), "shutdown running accepted");
        until = Clock::now() + 1s;
        while (!scheduler.active() && Clock::now() < until) std::this_thread::yield();
        check(scheduler.submit({"pending", "hello"}, Clock::now() + 2s, pending), "shutdown pending accepted");
        scheduler.stop();
        check(pending.get().response.status == "unavailable", "pending released at shutdown");
        check(running.get().response.status == "unavailable", "active released at shutdown");
        check(!scheduler.submit({"after-stop", "hello"}, Clock::now() + 1s, refused), "no post-stop admission");
        std::cout << "All scheduler tests passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
