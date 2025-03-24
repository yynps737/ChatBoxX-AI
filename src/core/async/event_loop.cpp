#include "core/async/event_loop.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::async {

EventLoop::EventLoop(size_t thread_count)
    : running_(false) {
    thread_count_ = thread_count > 0 ? thread_count : std::thread::hardware_concurrency();
}

EventLoop::~EventLoop() {
    Stop();
}

void EventLoop::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }
    
    running_ = true;
    
    work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        io_context_.get_executor());
    
    for (size_t i = 0; i < thread_count_; ++i) {
        worker_threads_.emplace_back([this] {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                spdlog::error("Exception in EventLoop worker thread: {}", e.what());
            }
        });
    }
    
    spdlog::info("EventLoop started with {} worker threads", thread_count_);
}

void EventLoop::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    work_guard_.reset();
    io_context_.stop();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    io_context_.restart();
    
    spdlog::info("EventLoop stopped");
}

boost::asio::io_context& EventLoop::GetIoContext() {
    return io_context_;
}

EventLoop& EventLoop::GetInstance() {
    static EventLoop instance;
    return instance;
}

template <typename Task>
void EventLoop::Post(Task&& task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    boost::asio::post(io_context_, std::forward<Task>(task));
}

template <typename Task>
void EventLoop::Dispatch(Task&& task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    boost::asio::dispatch(io_context_, std::forward<Task>(task));
}

template <typename Task>
void EventLoop::Defer(Task&& task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    boost::asio::defer(io_context_, std::forward<Task>(task));
}

template <typename Clock, typename Duration>
void EventLoop::ScheduleAt(const std::chrono::time_point<Clock, Duration>& time_point, std::function<void()> task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, time_point);
    
    timer->async_wait([timer, task](const boost::system::error_code& ec) {
        if (!ec) {
            task();
        }
    });
}

template <typename Rep, typename Period>
void EventLoop::ScheduleAfter(const std::chrono::duration<Rep, Period>& duration, std::function<void()> task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, duration);
    
    timer->async_wait([timer, task](const boost::system::error_code& ec) {
        if (!ec) {
            task();
        }
    });
}

void EventLoop::ScheduleRecurring(std::chrono::milliseconds interval, std::function<void()> task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    struct RecurringTask : std::enable_shared_from_this<RecurringTask> {
        RecurringTask(boost::asio::io_context& io_context, std::chrono::milliseconds interval, std::function<void()> task)
            : timer(io_context), interval(interval), task(std::move(task)) {
        }
        
        void Start() {
            timer.expires_after(interval);
            
            timer.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
                if (!ec) {
                    self->task();
                    self->Start();
                }
            });
        }
        
        boost::asio::steady_timer timer;
        std::chrono::milliseconds interval;
        std::function<void()> task;
    };
    
    auto recurring_task = std::make_shared<RecurringTask>(io_context_, interval, std::move(task));
    recurring_task->Start();
}

} // namespace ai_backend::core::async