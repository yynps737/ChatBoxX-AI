#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "core/http/request.h"
#include "core/async/task.h"

namespace ai_backend::api::middlewares {

class RateLimiter {
public:
    RateLimiter();
    ~RateLimiter();
    
    core::async::Task<bool> Process(core::http::Request& request);

private:
    core::async::Task<bool> CheckRateLimit(const std::string& client_id);
    void InitCleanupTask();

private:
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> request_history_;
    std::mutex mutex_;
    std::atomic<bool> shutdown_{false};
    std::thread cleanup_thread_;
    int max_requests_per_minute_;
    int max_requests_per_hour_;
    bool enabled_;
};

} // namespace ai_backend::api::middlewares