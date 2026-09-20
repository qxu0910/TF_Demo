#include "scheduler.hpp"
#include <stdexcept>

namespace factory {
namespace {
double ms(Scheduler::Clock::time_point from, Scheduler::Clock::time_point to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
}
Result failed(const Request& request, const char* status, const char* error) {
    return {{request.request_id, status, "", error}, 0, 0};
}
}

Scheduler::Scheduler(std::size_t workers, std::size_t capacity, int work_ms)
    : capacity_(capacity), work_ms_(work_ms) {
    if (workers == 0 || capacity == 0 || work_ms < 0) throw std::invalid_argument("invalid scheduler settings");
    try {
        for (std::size_t i = 0; i < workers; ++i) workers_.emplace_back([this] { run(); });
    } catch (...) { stop(); for (auto& thread : workers_) thread.join(); throw; }
}

Scheduler::~Scheduler() {
    stop();
    for (auto& thread : workers_) thread.join();
}

bool Scheduler::submit(Request request, Clock::time_point deadline, std::future<Result>& future) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_ || queue_.size() >= capacity_) { ++rejected_; return false; }
    Job job{std::move(request), Clock::now(), deadline, {}};
    future = job.promise.get_future();
    queue_.push_back(std::move(job));
    available_.notify_one();
    return true;
}

void Scheduler::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    for (auto& job : queue_) job.promise.set_value(failed(job.request, "unavailable", "runtime is stopping"));
    queue_.clear();
    available_.notify_all();
}

std::size_t Scheduler::queued() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void Scheduler::run() {
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            available_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_) return;
            job = std::move(queue_.front());
            queue_.pop_front();
            ++active_;
        }
        const auto started = Clock::now();
        Result result;
        try {
            // 固定演示延迟仅由启动参数控制，便于复现实验；不是模型计算性能。
            const auto work_end = started + std::chrono::milliseconds(work_ms_);
            while (!stopping_ && Clock::now() < job.deadline && Clock::now() < work_end)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (stopping_) result = failed(job.request, "unavailable", "runtime is stopping");
            else if (Clock::now() >= job.deadline) result = failed(job.request, "timeout", "runtime deadline exceeded");
            else result.response = runtime_.handle(job.request);
            if (result.response.status == "success" && Clock::now() >= job.deadline)
                result = failed(job.request, "timeout", "runtime deadline exceeded");
        } catch (...) { result = failed(job.request, "internal_error", "compute failed"); }
        result.queue_ms = ms(job.submitted, started);
        result.compute_ms = ms(started, Clock::now());
        if (result.response.status == "success") ++completed_;
        if (result.response.status == "timeout") ++timed_out_;
        --active_;
        job.promise.set_value(std::move(result));
    }
}
} // namespace factory
