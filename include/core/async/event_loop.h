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
    
private:
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::vector<std::thread> worker_threads_;
    size_t thread_count_;
    std::atomic<bool> running_;
    std::mutex mutex_;
};

} // namespace ai_backend::core::async