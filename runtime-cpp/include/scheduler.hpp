#pragma once

#include "runtime.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace factory {

struct Result {
    Response response;
    double queue_ms = 0;
    double compute_ms = 0;
};

// L7：HTTP 线程只提交任务并等待 future；计算由固定工作线程执行。
class Scheduler {
public:
    using Clock = std::chrono::steady_clock;
    Scheduler(std::size_t workers, std::size_t capacity, int work_ms = 0);
    ~Scheduler();
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // false 表示队列满或停止接单；成功时 future 对应唯一结果。
    bool submit(Request request, Clock::time_point deadline, std::future<Result>& future);
    void stop();
    std::size_t queued() const;
    std::size_t active() const { return active_.load(); }
    std::size_t completed() const { return completed_.load(); }
    std::size_t rejected() const { return rejected_.load(); }
    std::size_t timed_out() const { return timed_out_.load(); }

private:
    struct Job {
        Request request;
        Clock::time_point submitted;
        Clock::time_point deadline;
        std::promise<Result> promise;
    };
    void run();
    const std::size_t capacity_;
    const int work_ms_;
    Runtime runtime_;
    mutable std::mutex mutex_;
    std::condition_variable available_;
    std::deque<Job> queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::size_t> active_{0}, completed_{0}, rejected_{0}, timed_out_{0};
};

} // namespace factory
