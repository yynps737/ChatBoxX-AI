#include "api/middlewares/rate_limiter.h"
#include <spdlog/spdlog.h>
#include "core/config/config_manager.h"
#include <chrono>
#include <map>

namespace ai_backend::api::middlewares {

using namespace core::async;

RateLimiter::RateLimiter() {
    // 从配置中加载速率限制设置
    auto& config = core::config::ConfigManager::GetInstance();
    max_requests_per_minute_ = config.GetInt("rate_limit.max_requests_per_minute", 60);
    max_requests_per_hour_ = config.GetInt("rate_limit.max_requests_per_hour", 1000);
    max_requests_per_day_ = config.GetInt("rate_limit.max_requests_per_day", 10000);
    enabled_ = config.GetBool("rate_limit.enabled", true);
    
    // 基于IP的速率限制阈值可以更严格
    ip_max_requests_per_minute_ = config.GetInt("rate_limit.ip_max_per_minute", 30);
    
    // 初始化清理任务
    InitCleanupTask();
}

Task<bool> RateLimiter::Process(core::http::Request& request) {
    if (!enabled_) {
        co_return true;
    }
    
    // 获取客户端IP
    std::string client_ip = request.GetHeader("X-Forwarded-For");
    if (client_ip.empty()) {
        client_ip = request.GetHeader("X-Real-IP");
    }
    if (client_ip.empty()) {
        client_ip = request.client_ip;
    }
    
    // 解析X-Forwarded-For中的第一个IP
    size_t comma_pos = client_ip.find(',');
    if (comma_pos != std::string::npos) {
        client_ip = client_ip.substr(0, comma_pos);
    }
    
    // 检查IP是否在白名单中
    if (IsWhitelisted(client_ip)) {
        co_return true;
    }
    
    // 获取用户ID（如果已认证）
    std::string client_id = request.user_id.has_value() ? request.user_id.value() : "ip:" + client_ip;
    
    // 如果是API请求，可以进行更严格的限制
    bool is_api_request = request.path.find("/api/") == 0;
    int limit_multiplier = is_api_request ? 1 : 2; // API请求的限制更严格
    
    // 检查并更新速率限制
    bool allowed = co_await CheckRateLimit(client_id, client_ip, limit_multiplier);
    
    if (!allowed) {
        spdlog::warn("Rate limit exceeded for client: {}", client_id);
        
        // 生成服务重新可用的预计时间
        auto retry_after = CalculateRetryAfter(client_id);
        
        // 设置响应头
        request.rate_limit_headers["X-RateLimit-Limit"] = std::to_string(max_requests_per_minute_);
        request.rate_limit_headers["X-RateLimit-Remaining"] = "0";
        request.rate_limit_headers["Retry-After"] = std::to_string(retry_after);
        
        co_return false; // 拒绝请求
    }
    
    co_return true;
}

int RateLimiter::CalculateRetryAfter(const std::string& client_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto minute_ago = now - std::chrono::minutes(1);
    
    // 找到最早的请求时间
    std::chrono::time_point<std::chrono::steady_clock> earliest_request;
    
    if (!request_history_[client_id].empty()) {
        earliest_request = request_history_[client_id].front();
        
        // 计算到最早请求过期还需要多少秒
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            earliest_request + std::chrono::minutes(1) - now);
        
        return std::max(1, static_cast<int>(duration.count()));
    }
    
    return 60; // 默认1分钟
}

bool RateLimiter::IsWhitelisted(const std::string& ip) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 简单实现：检查IP是否在白名单中
    static const std::unordered_set<std::string> whitelist = {
        "127.0.0.1",
        "::1"
    };
    
    return whitelist.find(ip) != whitelist.end();
}

Task<bool> RateLimiter::CheckRateLimit(const std::string& client_id, const std::string& client_ip, int limit_multiplier) {
    auto now = std::chrono::steady_clock::now();
    auto minute_ago = now - std::chrono::minutes(1);
    auto hour_ago = now - std::chrono::hours(1);
    auto day_ago = now - std::chrono::hours(24);
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 添加当前请求
    request_history_[client_id].push_back(now);
    
    // 如果是IP限制，也添加到IP历史记录中
    if (client_id.find("ip:") == 0) {
        ip_request_history_[client_ip].push_back(now);
    }
    
    // 计算最近一分钟/小时/天的请求数
    int minute_requests = 0;
    int hour_requests = 0;
    int day_requests = 0;
    
    for (const auto& timestamp : request_history_[client_id]) {
        if (timestamp >= minute_ago) {
            minute_requests++;
        }
        if (timestamp >= hour_ago) {
            hour_requests++;
        }
        if (timestamp >= day_ago) {
            day_requests++;
        }
    }
    
    // 特别检查IP限制（防止未认证用户滥用）
    int ip_minute_requests = 0;
    if (client_id.find("ip:") == 0 && ip_request_history_.find(client_ip) != ip_request_history_.end()) {
        for (const auto& timestamp : ip_request_history_[client_ip]) {
            if (timestamp >= minute_ago) {
                ip_minute_requests++;
            }
        }
    }
    
    // 检查是否超过限制
    if (minute_requests > max_requests_per_minute_ * limit_multiplier || 
        hour_requests > max_requests_per_hour_ * limit_multiplier ||
        day_requests > max_requests_per_day_ * limit_multiplier ||
        (client_id.find("ip:") == 0 && ip_minute_requests > ip_max_requests_per_minute_)) {
        
        // 移除当前请求
        request_history_[client_id].pop_back();
        if (client_id.find("ip:") == 0) {
            ip_request_history_[client_ip].pop_back();
        }
        
        co_return false;
    }
    
    co_return true;
}
}