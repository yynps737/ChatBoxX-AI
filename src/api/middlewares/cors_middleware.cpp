#include "api/middlewares/cors_middleware.h"
#include <spdlog/spdlog.h>
#include "core/config/config_manager.h"

namespace ai_backend::api::middlewares {

using namespace core::async;

CorsMiddleware::CorsMiddleware() {
    // 从配置中加载允许的源
    auto& config = core::config::ConfigManager::GetInstance();
    allowed_origins_ = config.GetStringList("cors.allowed_origins", {"*"});
    allowed_methods_ = config.GetStringList("cors.allowed_methods", {"GET", "POST", "PUT", "DELETE", "OPTIONS"});
    allowed_headers_ = config.GetStringList("cors.allowed_headers", {"Content-Type", "Authorization"});
    allow_credentials_ = config.GetBool("cors.allow_credentials", true);
    max_age_ = config.GetInt("cors.max_age", 86400);
}

Task<bool> CorsMiddleware::Process(core::http::Request& request) {
    // 获取Origin头
    std::string origin = request.GetHeader("Origin");
    
    // 如果没有Origin头，不需要处理CORS
    if (origin.empty()) {
        co_return true;
    }
    
    // 检查是否允许该源
    bool origin_allowed = false;
    if (allowed_origins_.size() == 1 && allowed_origins_[0] == "*") {
        origin_allowed = true;
    } else {
        for (const auto& allowed_origin : allowed_origins_) {
            if (origin == allowed_origin) {
                origin_allowed = true;
                break;
            }
        }
    }
    
    if (!origin_allowed) {
        spdlog::warn("CORS: Origin not allowed: {}", origin);
        co_return true; // 继续处理，浏览器会拒绝非允许的跨域请求
    }
    
    // 设置CORS响应头
    request.cors_headers["Access-Control-Allow-Origin"] = origin;
    
    // 设置允许的方法
    std::string methods_str;
    for (const auto& method : allowed_methods_) {
        if (!methods_str.empty()) {
            methods_str += ", ";
        }
        methods_str += method;
    }
    request.cors_headers["Access-Control-Allow-Methods"] = methods_str;
    
    // 设置允许的头部
    std::string headers_str;
    for (const auto& header : allowed_headers_) {
        if (!headers_str.empty()) {
            headers_str += ", ";
        }
        headers_str += header;
    }
    request.cors_headers["Access-Control-Allow-Headers"] = headers_str;
    
    // 设置凭证标志
    request.cors_headers["Access-Control-Allow-Credentials"] = allow_credentials_ ? "true" : "false";
    
    // 设置缓存时间
    request.cors_headers["Access-Control-Max-Age"] = std::to_string(max_age_);
    
    // 处理预检请求
    if (request.method == "OPTIONS") {
        request.is_preflight = true;
    }
    
    co_return true;
}

} // namespace ai_backend::api::middlewares