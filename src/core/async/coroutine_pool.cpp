#include "core/async/coroutine_pool.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace ai_backend::core::async {

// Make TaskWrapper::execute() a pure virtual function instead of empty implementation
class TaskWrapper {
public:
    virtual ~TaskWrapper() = default;
    virtual void execute() = 0; // Changed to pure virtual
};

template <typename Func>
class ConcreteTask : public TaskWrapper {
public:
    explicit ConcreteTask(Func&& func) : func_(std::forward<Func>(func)) {}
    
    void execute() override {
        func_();
    }
    
private:
    Func func_;
};

CoroutinePool::CoroutinePool(size_t max_coroutines, size_t thread_count)
    : max_coroutines_(max_coroutines),
      active_coroutines_(0),
      should_terminate_(false) {
    
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
    }
    
    for (size_t i = 0; i < thread_count; ++i) {
        worker_threads_.emplace_back(&CoroutinePool::WorkerThread, this);
    }
    
    // Initialize health check timer
    StartHealthCheck();
}

CoroutinePool::~CoroutinePool() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        should_terminate_ = true;
        task_cv_.notify_all();
    }
    
    // Join worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void CoroutinePool::WaitAll() {
    std::unique_lock<std::mutex> lock(mutex_);
    completion_cv_.wait(lock, [this]() {
        return task_queue_.empty() && active_coroutines_ == 0;
    });
}

size_t CoroutinePool::GetActiveCount() const {
    return active_coroutines_.load();
}

// New methods for health monitoring
size_t CoroutinePool::GetPendingTaskCount() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return task_queue_.size();
}

float CoroutinePool::GetUtilizationRate() const {
    return static_cast<float>(active_coroutines_) / worker_threads_.size();
}

void CoroutinePool::StartHealthCheck() {
    health_check_thread_ = std::thread([this]() {
        while (!should_terminate_) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            PerformHealthCheck();
        }
    });
}

void CoroutinePool::PerformHealthCheck() {
    // Log metrics for monitoring
    float utilization = GetUtilizationRate();
    size_t pending_tasks = GetPendingTaskCount();
    
    spdlog::debug("CoroutinePool health: Utilization={}%, Pending tasks={}, Active tasks={}",
                  utilization * 100.0f, pending_tasks, active_coroutines_.load());
    
    // Adaptive scaling based on utilization
    if (utilization > 0.8f && worker_threads_.size() < max_coroutines_) {
        // High utilization, add more workers
        AddWorkers(std::min(2UL, max_coroutines_ - worker_threads_.size()));
    } else if (utilization < 0.2f && worker_threads_.size() > 2) {
        // Low utilization, reduce workers
        RemoveWorkers(1);
    }
}

void CoroutinePool::AddWorkers(size_t count) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    spdlog::info("CoroutinePool: Adding {} worker threads", count);
    
    for (size_t i = 0; i < count; ++i) {
        worker_threads_.emplace_back(&CoroutinePool::WorkerThread, this);
    }
}

void CoroutinePool::RemoveWorkers(size_t count) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    count = std::min(count, worker_threads_.size() - 2);  // Keep at least 2 threads
    if (count == 0) return;
    
    spdlog::info("CoroutinePool: Removing {} worker threads", count);
    
    // Flag threads for removal
    workers_to_remove_ = count;
    task_cv_.notify_all();
}

void CoroutinePool::WorkerThread() {
    while (true) {
        std::unique_ptr<TaskWrapper> task;
        bool should_exit = false;
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            task_cv_.wait(lock, [this]() {
                return !task_queue_.empty() || should_terminate_ || workers_to_remove_ > 0;
            });
            
            if (should_terminate_) {
                break;
            }
            
            if (workers_to_remove_ > 0) {
                --workers_to_remove_;
                should_exit = true;
            } else if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop_front();
                ++active_coroutines_;
            }
        }
        
        if (should_exit) {
            spdlog::debug("CoroutinePool worker exiting as part of scaling down");
            break;
        }
        
        if (task) {
            try {
                // Execute with timeout protection
                auto future = std::async(std::launch::async, [&task]() {
                    task->execute();
                });
                
                auto status = future.wait_for(std::chrono::seconds(60));
                if (status == std::future_status::timeout) {
                    spdlog::error("Task execution timed out after 60 seconds");
                }
            } catch (const std::exception& e) {
                spdlog::error("Exception in task execution: {}", e.what());
            }
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                --active_coroutines_;
                completion_cv_.notify_all();
            }
        }
    }
}

} // namespace ai_backend::core::async