#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <coroutine>
#include "task.h"

namespace ai_backend::core::async {

// 协程任务包装器
class TaskWrapper {
public:
    virtual ~TaskWrapper() = default;
    virtual void execute() = 0;
};

// 具体的协程任务
template <typename Func>
class ConcreteTask : public TaskWrapper {
public:
    explicit ConcreteTask(Func&& func) : func_(std::move(func)) {}
    
    void execute() override {
        func_();
    }
    
private:
    Func func_;
};

// 高性能协程池
class CoroutinePool {
public:
    CoroutinePool(size_t max_coroutines, size_t thread_count = 0)
        : max_coroutines_(max_coroutines),
          active_coroutines_(0),
          should_terminate_(false) {
        
        if (thread_count == 0) {
            thread_count = std::thread::hardware_concurrency();
        }
        
        for (size_t i = 0; i < thread_count; ++i) {
            worker_threads_.emplace_back(&CoroutinePool::WorkerThread, this);
        }
    }
    
    ~CoroutinePool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            should_terminate_ = true;
            task_cv_.notify_all();
        }
        
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    // 提交异步任务
    template <typename Func>
    auto Submit(Func&& func) {
        using ReturnType = std::invoke_result_t<Func>;
        
        std::promise<ReturnType> promise;
        std::future<ReturnType> future = promise.get_future();
        
        auto task = std::make_unique<ConcreteTask<std::function<void()>>>(
            [func = std::forward<Func>(func), p = std::move(promise)]() mutable {
                try {
                    if constexpr (std::is_void_v<ReturnType>) {
                        func();
                        p.set_value();
                    } else {
                        p.set_value(func());
                    }
                } catch (...) {
                    p.set_exception(std::current_exception());
                }
            }
        );
        
        EnqueueTask(std::move(task));
        return future;
    }
    
    // 等待所有任务完成
    void WaitAll() {
        std::unique_lock<std::mutex> lock(mutex_);
        completion_cv_.wait(lock, [this]() {
            return task_queue_.empty() && active_coroutines_ == 0;
        });
    }
    
    // 获取活跃协程数量
    size_t GetActiveCount() const {
        return active_coroutines_.load();
    }
    
    // 获取等待任务数量
    size_t GetPendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return task_queue_.size();
    }

private:
    void EnqueueTask(std::unique_ptr<TaskWrapper> task) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待直到有可用协程槽位
        task_cv_.wait(lock, [this]() {
            return active_coroutines_ < max_coroutines_ || should_terminate_;
        });
        
        if (should_terminate_) {
            throw std::runtime_error("CoroutinePool is shutting down");
        }
        
        task_queue_.push_back(std::move(task));
        task_cv_.notify_one();
    }
    
    void WorkerThread() {
        while (true) {
            std::unique_ptr<TaskWrapper> task;
            
            {
                std::unique_lock<std::mutex> lock(mutex_);
                
                task_cv_.wait(lock, [this]() {
                    return !task_queue_.empty() || should_terminate_;
                });
                
                if (should_terminate_ && task_queue_.empty()) {
                    break;
                }
                
                if (!task_queue_.empty()) {
                    task = std::move(task_queue_.front());
                    task_queue_.pop_front();
                    ++active_coroutines_;
                }
            }
            
            if (task) {
                task->execute();
                
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    --active_coroutines_;
                    completion_cv_.notify_all();
                }
            }
        }
    }

private:
    const size_t max_coroutines_;
    std::atomic<size_t> active_coroutines_;
    bool should_terminate_;
    mutable std::mutex mutex_;
    std::condition_variable task_cv_;
    std::condition_variable completion_cv_;
    std::deque<std::unique_ptr<TaskWrapper>> task_queue_;
    std::vector<std::thread> worker_threads_;
};

} // namespace ai_backend::core::async