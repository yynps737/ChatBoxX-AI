#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ai_backend::core::http {

// HTTP请求类
class Request {
public:
    Request() = default;
    
    // 请求方法 (GET, POST, PUT, DELETE等)
    std::string method;
    
    // 请求目标URL
    std::string target;
    
    // 路径部分 (不含查询参数)
    std::string path;
    
    // HTTP版本
    unsigned version;
    
    // 请求头
    std::unordered_map<std::string, std::string> headers;
    
    // 请求体
    std::string body;
    
    // 查询参数
    std::unordered_map<std::string, std::string> query_params;
    
    // 路径参数
    std::unordered_map<std::string, std::string> path_params;
    
    // 认证用户ID (由Auth中间件设置)
    std::optional<std::string> user_id;
    
    // 获取头部值，如果不存在返回空字符串
    std::string GetHeader(const std::string& name) const;
    
    // 获取查询参数，如果不存在返回默认值
    std::string GetQueryParam(const std::string& name, const std::string& default_value = "") const;
    
    // 获取路径参数，如果不存在返回默认值
    std::string GetPathParam(const std::string& name, const std::string& default_value = "") const;
    
    // 检查头部是否存在
    bool HasHeader(const std::string& name) const;
    
    // 检查查询参数是否存在
    bool HasQueryParam(const std::string& name) const;
    
    // 检查路径参数是否存在
    bool HasPathParam(const std::string& name) const;
};

} // namespace ai_backend::core::http