#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace ai_backend::core::http {

// 流式写入器
class StreamWriter {
public:
    virtual ~StreamWriter() = default;
    
    // 写入数据
    virtual void Write(const std::string& data) = 0;
    
    // 结束写入
    virtual void End() = 0;
};

// HTTP响应类
class Response {
public:
    Response() = default;
    
    // 状态码
    int status_code = 200;
    
    // 响应头
    std::unordered_map<std::string, std::string> headers;
    
    // 响应体
    std::string body;
    
    // 流式响应处理函数
    std::function<core::async::Task<void>(StreamWriter&)> stream_handler;
    
    // 创建JSON响应
    static Response WithJson(int status_code, const nlohmann::json& json_data);
    
    // 设置头部
    Response& SetHeader(const std::string& name, const std::string& value);
    
    // 常见状态码的响应工厂方法
    static Response OK(const nlohmann::json& data = {});
    static Response Created(const nlohmann::json& data = {});
    static Response BadRequest(const nlohmann::json& error = {});
    static Response Unauthorized(const nlohmann::json& error = {});
    static Response Forbidden(const nlohmann::json& error = {});
    static Response NotFound(const nlohmann::json& error = {});
    static Response InternalServerError(const nlohmann::json& error = {});
};

} // namespace ai_backend::core::http