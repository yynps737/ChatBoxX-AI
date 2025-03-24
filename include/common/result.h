#pragma once

#include <string>
#include <utility>
#include <variant>

namespace ai_backend::common {

// 通用结果类，用于表示操作的成功或失败
template <typename T>
class Result {
public:
    // 创建成功结果
    static Result<T> Ok(T value) {
        return Result<T>(std::move(value));
    }
    
    // 创建失败结果
    static Result<T> Error(std::string error) {
        return Result<T>(std::move(error));
    }
    
    // 检查结果是否成功
    bool IsOk() const {
        return result_.index() == 0;
    }
    
    // 检查结果是否失败
    bool IsError() const {
        return result_.index() == 1;
    }
    
    // 获取结果值（如果成功）
    const T& GetValue() const {
        if (IsError()) {
            throw std::runtime_error("Cannot get value from error result: " + GetError());
        }
        return std::get<0>(result_);
    }
    
    // 获取可修改的结果值
    T& GetValue() {
        if (IsError()) {
            throw std::runtime_error("Cannot get value from error result: " + GetError());
        }
        return std::get<0>(result_);
    }
    
    // 获取错误消息（如果失败）
    const std::string& GetError() const {
        if (IsOk()) {
            throw std::runtime_error("Cannot get error from successful result");
        }
        return std::get<1>(result_);
    }
    
    // 映射结果到另一个类型
    template <typename U, typename Func>
    Result<U> Map(Func&& func) const {
        if (IsOk()) {
            return Result<U>::Ok(func(GetValue()));
        } else {
            return Result<U>::Error(GetError());
        }
    }
    
    // 执行副作用操作
    template <typename Func>
    Result<T>& Then(Func&& func) {
        if (IsOk()) {
            func(GetValue());
        }
        return *this;
    }
    
    // 执行错误处理
    template <typename Func>
    Result<T>& Catch(Func&& func) {
        if (IsError()) {
            func(GetError());
        }
        return *this;
    }
    
    // 提供默认值
    T ValueOr(T default_value) const {
        if (IsOk()) {
            return GetValue();
        }
        return default_value;
    }

private:
    // 存储成功值或错误消息
    std::variant<T, std::string> result_;
    
    // 私有构造函数
    explicit Result(T value) : result_(std::move(value)) {}
    explicit Result(std::string error) : result_(std::move(error)) {}
};

// Result<void>的特化版本
template <>
class Result<void> {
public:
    static Result<void> Ok() {
        return Result<void>(true);
    }
    
    static Result<void> Error(std::string error) {
        return Result<void>(std::move(error));
    }
    
    bool IsOk() const {
        return success_;
    }
    
    bool IsError() const {
        return !success_;
    }
    
    const std::string& GetError() const {
        if (IsOk()) {
            throw std::runtime_error("Cannot get error from successful result");
        }
        return error_;
    }
    
    template <typename Func>
    Result<void>& Then(Func&& func) {
        if (IsOk()) {
            func();
        }
        return *this;
    }
    
    template <typename Func>
    Result<void>& Catch(Func&& func) {
        if (IsError()) {
            func(error_);
        }
        return *this;
    }
    
    template <typename T, typename Func>
    Result<T> Map(Func&& func) const {
        if (IsOk()) {
            return Result<T>::Ok(func());
        } else {
            return Result<T>::Error(error_);
        }
    }

private:
    bool success_;
    std::string error_;
    
    explicit Result(bool success) : success_(success), error_("") {}
    explicit Result(std::string error) : success_(false), error_(std::move(error)) {}
};

} // namespace ai_backend::common