#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

namespace ai_backend::core::async {

struct EventLoopMetrics {
    std::atomic<size_t> tasks_posted{0};
    std::atomic<size_t> tasks_dispatched{0};
    std::atomic<size_t> tasks_deferred{0};
    std::atomic<size_t> tasks_scheduled{0};
    std::atomic<size_t> scheduled_tasks_executed{0};
    std::atomic<size_t> recurring_tasks_created{0};
    std::atomic<size_t> recurring_tasks_executed{0};
    std::atomic<size_t> exception_count{0};
    std::atomic<size_t> timer_errors{0};
};

class EventLoop {
public:
    EventLoop(size_t thread_count = 0);
    ~EventLoop();
    
    void Start();
    void Stop();
    
    boost::asio::io_context& GetIoContext();
    
    static EventLoop& GetInstance();
    
    template <typename Task>
    void Post(Task&& task);
    
    template <typename Task>
    void Dispatch(Task&& task);
    
    template <typename Task>
    void Defer(Task&& task);
    
    template <typename Clock, typename Duration>
    void ScheduleAt(const std::chrono::time_point<Clock, Duration>& time_point, std::function<void()> task);
    
    template <typename Rep, typename Period>
    void ScheduleAfter(const std::chrono::duration<Rep, Period>& duration, std::function<void()> task);
    
    void ScheduleRecurring(std::chrono::milliseconds interval, std::function<void()> task);
    
    bool IsMetricsCollectionEnabled() const;
    EventLoopMetrics GetMetrics() const;
    void ResetMetrics();

private:
    void InitializeMetrics();
    void StartMetricsCollection();

private:
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::vector<std::thread> worker_threads_;
    size_t thread_count_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::thread metrics_thread_;
    std::atomic<bool> metrics_collection_enabled_;
    EventLoopMetrics metrics_;
};

} // namespace ai_backend::core::async