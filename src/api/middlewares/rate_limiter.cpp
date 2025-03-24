#include "api/middlewares/rate_limiter.h"
#include <spdlog/spdlog.h>
#include "core/config/config_manager.h"

namespace ai_backend::api::middlewares {

using namespace core::async;

RateLimiter::RateLimiter() {
    // 从配置中加载速率限制设置
    auto& config = core::config::ConfigManager::GetInstance();
    max_requests_per_minute_ = config.GetInt("rate_limit.max_requests_per_minute", 60);
    max_requests_per_hour_ = config.GetInt("rate_limit.max_requests_per_hour", 1000);
    enabled_ = config.GetBool("rate_limit.enabled", true);
    
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
        client_ip = request.client_ip;
    }
    
    // 获取用户ID（如果已认证）
    std::string client_id = request.user_id.has_value() ? request.user_id.value() : client_ip;
    
    // 检查并更新速率限制
    bool allowed = co_await CheckRateLimit(client_id);
    
    if (!allowed) {
        spdlog::warn("Rate limit exceeded for client: {}", client_id);
        
        // 设置响应头
        request.rate_limit_headers["X-RateLimit-Limit"] = std::to_string(max_requests_per_minute_);
        request.rate_limit_headers["X-RateLimit-Remaining"] = "0";
        request.rate_limit_headers["Retry-After"] = "60";
        
        co_return false; // 拒绝请求
    }
    
    co_return true;
}

Task<bool> RateLimiter::CheckRateLimit(const std::string& client_id) {
    auto now = std::chrono::steady_clock::now();
    auto minute_ago = now - std::chrono::minutes(1);
    auto hour_ago = now - std::chrono::hours(1);
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 添加当前请求
    request_history_[client_id].push_back(now);
    
    // 计算最近一分钟的请求数
    int minute_requests = 0;
    int hour_requests = 0;
    
    for (const auto& timestamp : request_history_[client_id]) {
        if (timestamp >= minute_ago) {
            minute_requests++;
        }
        if (timestamp >= hour_ago) {
            hour_requests++;
        }
    }
    
    // 检查是否超过限制
    if (minute_requests > max_requests_per_minute_ || hour_requests > max_requests_per_hour_) {
        // 移除当前请求
        request_history_[client_id].pop_back();
        co_return false;
    }
    
    co_return true;
}

void RateLimiter::InitCleanupTask() {
    cleanup_thread_ = std::thread([this]() {
        while (!shutdown_) {
            std::this_thread::sleep_for(std::chrono::minutes(5));
            
            auto now = std::chrono::steady_clock::now();
            auto hour_ago = now - std::chrono::hours(1);
            
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 清理过期的请求记录
            for (auto it = request_history_.begin(); it != request_history_.end();) {
                auto& timestamps = it->second;
                
                // 移除超过1小时的记录
                timestamps.erase(
                    std::remove_if(timestamps.begin(), timestamps.end(),
                        [&hour_ago](const auto& timestamp) {
                            return timestamp < hour_ago;
                        }
                    ),
                    timestamps.end()
                );
                
                // 如果没有记录，移除此客户端
                if (timestamps.empty()) {
                    it = request_history_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    });
}

RateLimiter::~RateLimiter() {
    shutdown_ = true;
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

} // namespace ai_backend::api::middlewares