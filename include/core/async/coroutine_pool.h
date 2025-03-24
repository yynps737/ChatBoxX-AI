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

class TaskWrapper {
public:
    virtual ~TaskWrapper() = default;
    virtual void execute() = 0;
};

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

class CoroutinePool {
public:
    CoroutinePool(size_t max_coroutines, size_t thread_count = 0);
    ~CoroutinePool();
    
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
    
    void WaitAll();
    size_t GetActiveCount() const;
    size_t GetPendingCount() const;
    size_t GetPendingTaskCount() const;
    float GetUtilizationRate() const;
    void AddWorkers(size_t count);
    void RemoveWorkers(size_t count);

private:
    void EnqueueTask(std::unique_ptr<TaskWrapper> task);
    void WorkerThread();
    void StartHealthCheck();
    void PerformHealthCheck();

private:
    const size_t max_coroutines_;
    std::atomic<size_t> active_coroutines_;
    std::atomic<bool> should_terminate_;
    mutable std::mutex mutex_;
    std::condition_variable task_cv_;
    std::condition_variable completion_cv_;
    std::deque<std::unique_ptr<TaskWrapper>> task_queue_;
    std::vector<std::thread> worker_threads_;
    std::atomic<size_t> workers_to_remove_{0};
    std::thread health_check_thread_;
};

} // namespace ai_backend::core::async