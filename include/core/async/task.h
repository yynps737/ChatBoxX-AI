#pragma once

#include <coroutine>
#include <exception>
#include <functional>
#include <future>
#include <optional>
#include <variant>

namespace ai_backend::core::async {

// Task 类是协程的返回类型，用于支持 C++20 协程
template <typename T>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::variant<std::monostate, T, std::exception_ptr> result;
        std::coroutine_handle<> continuation; // 添加缺少的成员
        
        Task get_return_object() { 
            return Task(handle_type::from_promise(*this)); 
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(T value) { 
            result.template emplace<1>(std::move(value)); 
        }
        
        void unhandled_exception() { 
            result.template emplace<2>(std::current_exception()); 
        }

        // 处理协程未恢复的情况
        ~promise_type() {
            if (result.index() == 2) {
                // 记录未处理的异常
            }
        }
    };

    Task() noexcept : handle_(nullptr) {}
    Task(handle_type h) noexcept : handle_(h) {}
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    // 禁止复制
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 协程等待接口
    bool await_ready() const noexcept { 
        return !handle_ || handle_.done(); 
    }
    
    void await_suspend(std::coroutine_handle<> awaiting) const noexcept {
        // 保存当前协程句柄，并设置恢复函数
        handle_.promise().continuation = awaiting;
    }
    
    T await_resume() {
        if (!handle_) {
            throw std::runtime_error("Task has no valid coroutine handle");
        }
        
        auto& promise = handle_.promise();
        if (promise.result.index() == 1) {
            return std::get<1>(std::move(promise.result));
        } else if (promise.result.index() == 2) {
            std::rethrow_exception(std::get<2>(promise.result));
        } else {
            throw std::runtime_error("Task has no result");
        }
    }

private:
    handle_type handle_;
};

// 返回void的Task特化版本
template <>
class Task<void> {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::variant<std::monostate, std::exception_ptr> result;
        std::coroutine_handle<> continuation; // 添加缺少的成员
        
        Task get_return_object() { 
            return Task(handle_type::from_promise(*this)); 
        }
        
        std::suspend_never initial_suspend() { return {}; }
        
        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() const noexcept { return false; }
                
                std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                    auto& promise = h.promise();
                    if (promise.continuation) {
                        return promise.continuation;
                    }
                    return std::noop_coroutine();
                }
                
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }
        
        void return_void() {}
        
        void unhandled_exception() {
            result.template emplace<1>(std::current_exception());
        }
    };

    Task() noexcept : handle_(nullptr) {}
    Task(handle_type h) noexcept : handle_(h) {}
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    // 禁止复制
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // 协程等待接口
    bool await_ready() const noexcept { 
        return !handle_ || handle_.done(); 
    }
    
    void await_suspend(std::coroutine_handle<> awaiting) const noexcept {
        handle_.promise().continuation = awaiting;
    }
    
    void await_resume() {
        if (!handle_) {
            throw std::runtime_error("Task has no valid coroutine handle");
        }
        
        auto& promise = handle_.promise();
        if (promise.result.index() == 1) {
            std::rethrow_exception(std::get<1>(promise.result));
        }
    }

private:
    handle_type handle_;
};

// 增加超时任务封装
template <typename T>
class TimeoutTask {
public:
    TimeoutTask(Task<T>&& task, std::chrono::milliseconds timeout)
        : task_(std::move(task)), timeout_(timeout) {}

    bool await_ready() const { return task_.await_ready(); }
    
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) {
        // 实现超时逻辑
    }
    
    T await_resume() { return task_.await_resume(); }

private:
    Task<T> task_;
    std::chrono::milliseconds timeout_;
};

} // namespace ai_backend::core::async
