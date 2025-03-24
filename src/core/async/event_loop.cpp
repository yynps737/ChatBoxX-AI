#include "core/async/event_loop.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::async {

EventLoop::EventLoop(size_t thread_count)
    : running_(false),
      metrics_collection_enabled_(true) {
    thread_count_ = thread_count > 0 ? thread_count : std::thread::hardware_concurrency();
    InitializeMetrics();
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
                if (metrics_collection_enabled_) {
                    metrics_.exception_count++;
                }
            }
        });
    }
    
    // Start metrics collection
    if (metrics_collection_enabled_) {
        StartMetricsCollection();
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
    
    // Stop metrics collection
    if (metrics_thread_.joinable()) {
        metrics_thread_.join();
    }
    
    spdlog::info("EventLoop stopped");
}

boost::asio::io_context& EventLoop::GetIoContext() {
    return io_context_;
}

EventLoop& EventLoop::GetInstance() {
    static EventLoop instance;
    return instance;
}

// Explicit template instantiations to prevent linking errors
template void EventLoop::Post<std::function<void()>>(std::function<void()>&&);
template void EventLoop::Dispatch<std::function<void()>>(std::function<void()>&&);
template void EventLoop::Defer<std::function<void()>>(std::function<void()>&&);

template <typename Task>
void EventLoop::Post(Task&& task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    if (metrics_collection_enabled_) {
        metrics_.tasks_posted++;
    }
    
    boost::asio::post(io_context_, std::forward<Task>(task));
}

template <typename Task>
void EventLoop::Dispatch(Task&& task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    if (metrics_collection_enabled_) {
        metrics_.tasks_dispatched++;
    }
    
    boost::asio::dispatch(io_context_, std::forward<Task>(task));
}

template <typename Task>
void EventLoop::Defer(Task&& task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    if (metrics_collection_enabled_) {
        metrics_.tasks_deferred++;
    }
    
    boost::asio::defer(io_context_, std::forward<Task>(task));
}

template <typename Clock, typename Duration>
void EventLoop::ScheduleAt(const std::chrono::time_point<Clock, Duration>& time_point, std::function<void()> task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, time_point);
    
    timer->async_wait([this, timer, task = std::move(task)](const boost::system::error_code& ec) {
        if (!ec) {
            try {
                if (metrics_collection_enabled_) {
                    metrics_.scheduled_tasks_executed++;
                }
                task();
            } catch (const std::exception& e) {
                spdlog::error("Exception in scheduled task: {}", e.what());
                if (metrics_collection_enabled_) {
                    metrics_.exception_count++;
                }
            }
        } else if (ec != boost::asio::error::operation_aborted) {
            spdlog::error("Timer error in ScheduleAt: {}", ec.message());
            if (metrics_collection_enabled_) {
                metrics_.timer_errors++;
            }
        }
    });
    
    if (metrics_collection_enabled_) {
        metrics_.tasks_scheduled++;
    }
}

template <typename Rep, typename Period>
void EventLoop::ScheduleAfter(const std::chrono::duration<Rep, Period>& duration, std::function<void()> task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, duration);
    
    timer->async_wait([this, timer, task = std::move(task)](const boost::system::error_code& ec) {
        if (!ec) {
            try {
                if (metrics_collection_enabled_) {
                    metrics_.scheduled_tasks_executed++;
                }
                task();
            } catch (const std::exception& e) {
                spdlog::error("Exception in scheduled task: {}", e.what());
                if (metrics_collection_enabled_) {
                    metrics_.exception_count++;
                }
            }
        } else if (ec != boost::asio::error::operation_aborted) {
            spdlog::error("Timer error in ScheduleAfter: {}", ec.message());
            if (metrics_collection_enabled_) {
                metrics_.timer_errors++;
            }
        }
    });
    
    if (metrics_collection_enabled_) {
        metrics_.tasks_scheduled++;
    }
}

void EventLoop::ScheduleRecurring(std::chrono::milliseconds interval, std::function<void()> task) {
    if (!running_) {
        throw std::runtime_error("EventLoop not running");
    }
    
    struct RecurringTask : std::enable_shared_from_this<RecurringTask> {
        RecurringTask(EventLoop& event_loop, boost::asio::io_context& io_context, 
                      std::chrono::milliseconds interval, std::function<void()> task)
            : event_loop_(event_loop), timer(io_context), interval(interval), task(std::move(task)), 
              execution_count(0), first_execution_time(std::chrono::steady_clock::now()),
              last_execution_time(first_execution_time) {
        }
        
        void Start() {
            timer.expires_after(interval);
            
            timer.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
                if (!ec) {
                    try {
                        // Execute the task
                        self->task();
                        
                        // Update metrics
                        self->execution_count++;
                        auto now = std::chrono::steady_clock::now();
                        self->last_execution_time = now;
                        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - self->first_execution_time).count();
                        
                        if (self->event_loop_.IsMetricsCollectionEnabled()) {
                            self->event_loop_.metrics_.recurring_tasks_executed++;
                            
                            // Log recurring task metrics every 10 executions
                            if (self->execution_count % 10 == 0) {
                                spdlog::debug("Recurring task metrics: count={}, avg_interval={}ms", 
                                             self->execution_count,
                                             total_duration / self->execution_count);
                            }
                        }
                        
                        // Reschedule
                        self->Start();
                    } catch (const std::exception& e) {
                        spdlog::error("Exception in recurring task: {}", e.what());
                        if (self->event_loop_.IsMetricsCollectionEnabled()) {
                            self->event_loop_.metrics_.exception_count++;
                        }
                        
                        // Reschedule despite error
                        self->Start();
                    }
                } else if (ec != boost::asio::error::operation_aborted) {
                    spdlog::error("Timer error in recurring task: {}", ec.message());
                    if (self->event_loop_.IsMetricsCollectionEnabled()) {
                        self->event_loop_.metrics_.timer_errors++;
                    }
                    
                    // Reschedule despite error
                    self->Start();
                }
            });
        }
        
        EventLoop& event_loop_;
        boost::asio::steady_timer timer;
        std::chrono::milliseconds interval;
        std::function<void()> task;
        
        // Metrics
        size_t execution_count;
        std::chrono::steady_clock::time_point first_execution_time;
        std::chrono::steady_clock::time_point last_execution_time;
    };
    
    auto recurring_task = std::make_shared<RecurringTask>(*this, io_context_, interval, std::move(task));
    recurring_task->Start();
    
    if (metrics_collection_enabled_) {
        metrics_.recurring_tasks_created++;
    }
}

void EventLoop::InitializeMetrics() {
    metrics_ = {};
}

void EventLoop::StartMetricsCollection() {
    // Start a background thread for metrics collection
    metrics_thread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            
            if (!running_) break;
            
            // Log metrics
            spdlog::info("EventLoop metrics: posted={}, dispatched={}, deferred={}, scheduled={}, "
                         "executed_scheduled={}, executed_recurring={}, exceptions={}, timer_errors={}",
                         metrics_.tasks_posted.load(),
                         metrics_.tasks_dispatched.load(),
                         metrics_.tasks_deferred.load(),
                         metrics_.tasks_scheduled.load(),
                         metrics_.scheduled_tasks_executed.load(),
                         metrics_.recurring_tasks_executed.load(),
                         metrics_.exception_count.load(),
                         metrics_.timer_errors.load());
        }
    });
}

bool EventLoop::IsMetricsCollectionEnabled() const {
    return metrics_collection_enabled_;
}

EventLoopMetrics EventLoop::GetMetrics() const {
    return metrics_;
}

void EventLoop::ResetMetrics() {
    InitializeMetrics();
}

} // namespace ai_backend::core::async