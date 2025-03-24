#include "core/async/coroutine_pool.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace ai_backend::core::async {

void TaskWrapper::execute() {
}

template <typename Func>
ConcreteTask<Func>::ConcreteTask(Func&& func) : func_(std::forward<Func>(func)) {
}

template <typename Func>
void ConcreteTask<Func>::execute() {
    func_();
}

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
}

CoroutinePool::~CoroutinePool() {
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

void CoroutinePool::WaitAll() {
    std::unique_lock<std::mutex> lock(mutex_);
    completion_cv_.wait(lock, [this]() {
        return task_queue_.empty() && active_coroutines_ == 0;
    });
}

size_t CoroutinePool::GetActiveCount() const {
    return active_coroutines_.load();
}

void CoroutinePool::WorkerThread() {
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

} // namespace ai_backend::core::async