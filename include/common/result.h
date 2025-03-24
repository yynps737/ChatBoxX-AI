#pragma once

#include <string>
#include <utility>
#include <variant>
#include <type_traits>

namespace ai_backend::common {

// 通用结果类，用于表示操作的成功或失败
template <typename T>
class Result {
public:
    // 创建成功结果
    static Result<T> Ok(T value) {
        return Result<T>(std::move(value), false);
    }
    
    // 创建失败结果
    static Result<T> Error(std::string error) {
        return Result<T>(std::move(error), true);
    }
    
    // 检查结果是否成功
    bool IsOk() const {
        return !is_error_;
    }
    
    // 检查结果是否失败
    bool IsError() const {
        return is_error_;
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
    bool is_error_;
    
    // 私有构造函数，避免歧义
    Result(T value, bool is_error) : result_(std::move(value)), is_error_(is_error) {}
    Result(std::string error, bool is_error) : result_(std::move(error)), is_error_(is_error) {}
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

// 特化 std::string 版本以避免构造函数歧义
template <>
class Result<std::string> {
public:
    static Result<std::string> Ok(std::string value) {
        return Result<std::string>(std::move(value), false);
    }
    
    static Result<std::string> Error(std::string error) {
        return Result<std::string>(std::move(error), true);
    }
    
    bool IsOk() const { return !is_error_; }
    bool IsError() const { return is_error_; }
    
    const std::string& GetValue() const {
        if (IsError()) {
            throw std::runtime_error("Cannot get value from error result: " + value_);
        }
        return value_;
    }
    
    std::string& GetValue() {
        if (IsError()) {
            throw std::runtime_error("Cannot get value from error result: " + value_);
        }
        return value_;
    }
    
    const std::string& GetError() const {
        if (IsOk()) {
            throw std::runtime_error("Cannot get error from successful result");
        }
        return value_;
    }
    
    std::string ValueOr(std::string default_value) const {
        if (IsOk()) {
            return value_;
        }
        return default_value;
    }
    
    template <typename Func>
    Result<std::string>& Then(Func&& func) {
        if (IsOk()) {
            func(value_);
        }
        return *this;
    }
    
    template <typename Func>
    Result<std::string>& Catch(Func&& func) {
        if (IsError()) {
            func(value_);
        }
        return *this;
    }
    
    template <typename U, typename Func>
    Result<U> Map(Func&& func) const {
        if (IsOk()) {
            return Result<U>::Ok(func(value_));
        } else {
            return Result<U>::Error(value_);
        }
    }

private:
    std::string value_;
    bool is_error_;
    
    Result(std::string value, bool is_error) : value_(std::move(value)), is_error_(is_error) {}
};

} // namespace ai_backend::common
