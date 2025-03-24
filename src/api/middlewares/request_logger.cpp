#include "api/middlewares/request_logger.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace ai_backend::api::middlewares {

using namespace core::async;

RequestLogger::RequestLogger() {
    // 从配置中读取日志级别和格式
    auto& config = core::config::ConfigManager::GetInstance();
    log_level_ = config.GetString("log.request_level", "info");
    log_body_ = config.GetBool("log.request_body", false);
    log_headers_ = config.GetBool("log.request_headers", false);
}

Task<bool> RequestLogger::Process(core::http::Request& request) {
    // 记录请求开始时间
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 设置请求开始时间到请求属性中
    request.start_time = start_time;
    
    // 生成请求ID
    std::string request_id = GenerateRequestId();
    request.request_id = request_id;
    
    // 记录请求
    LogRequest(request);
    
    co_return true;
}

void RequestLogger::LogRequest(const core::http::Request& request) {
    // 构建基本日志消息
    std::string message = fmt::format(
        "Request [{}] {} {}",
        request.request_id,
        request.method,
        request.path
    );
    
    // 添加客户端IP
    std::string client_ip = request.GetHeader("X-Forwarded-For");
    if (client_ip.empty()) {
        client_ip = request.client_ip;
    }
    message += fmt::format(" from {}", client_ip);
    
    // 添加用户ID（如果有）
    if (request.user_id.has_value()) {
        message += fmt::format(" (User: {})", request.user_id.value());
    }
    
    // 记录请求头
    if (log_headers_) {
        message += "\nHeaders:";
        for (const auto& [name, value] : request.headers) {
            // 不记录敏感头部
            if (name == "Authorization" || name == "Cookie") {
                message += fmt::format("\n  {}: [REDACTED]", name);
            } else {
                message += fmt::format("\n  {}: {}", name, value);
            }
        }
    }
    
    // 记录请求体
    if (log_body_ && !request.body.empty()) {
        // 限制请求体大小以避免日志过大
        const size_t max_body_length = 1000;
        std::string body = request.body;
        if (body.length() > max_body_length) {
            body = body.substr(0, max_body_length) + "...";
        }
        
        // 检测并脱敏JSON请求体中的敏感字段
        if (request.GetHeader("Content-Type").find("application/json") != std::string::npos) {
            try {
                auto json = nlohmann::json::parse(body);
                SanitizeJsonObject(json);
                body = json.dump(2);
            } catch (...) {
                // 解析失败，使用原始内容
            }
        }
        
        message += fmt::format("\nBody:\n{}", body);
    }
    
    // 根据配置的级别记录日志
    if (log_level_ == "trace") {
        spdlog::trace(message);
    } else if (log_level_ == "debug") {
        spdlog::debug(message);
    } else if (log_level_ == "info") {
        spdlog::info(message);
    } else if (log_level_ == "warn") {
        spdlog::warn(message);
    } else {
        spdlog::info(message);
    }
}

void RequestLogger::LogResponse(const core::http::Request& request, const core::http::Response& response, std::chrono::milliseconds duration) {
    // 构建基本日志消息
    std::string message = fmt::format(
        "Response [{}] {} {} - {} in {}ms",
        request.request_id,
        request.method,
        request.path,
        response.status_code,
        duration.count()
    );
    
    // 根据配置的级别记录日志
    if (log_level_ == "trace") {
        spdlog::trace(message);
    } else if (log_level_ == "debug") {
        spdlog::debug(message);
    } else if (log_level_ == "info") {
        spdlog::info(message);
    } else if (log_level_ == "warn") {
        spdlog::warn(message);
    } else {
        spdlog::info(message);
    }
}

std::string RequestLogger::GenerateRequestId() {
    static std::atomic<uint64_t> counter{0};
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t count = counter.fetch_add(1, std::memory_order_relaxed);
    uint64_t random = dis(gen);
    
    // 混合计数器和随机数生成请求ID
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << (count & 0xFFFFFFFF);
    ss << std::hex << std::setw(8) << std::setfill('0') << (random & 0xFFFFFFFF);
    
    return ss.str();
}

void RequestLogger::SanitizeJsonObject(nlohmann::json& json) {
    if (!json.is_object()) {
        return;
    }
    
    // 敏感字段列表
    static const std::unordered_set<std::string> sensitive_fields = {
        "password", "token", "secret", "key", "authorization", "auth", "credential"
    };
    
    // 对每个字段进行脱敏
    for (auto it = json.begin(); it != json.end(); ++it) {
        const std::string& key = it.key();
        
        // 检查字段名是否敏感
        bool is_sensitive = false;
        for (const auto& field : sensitive_fields) {
            if (key.find(field) != std::string::npos) {
                is_sensitive = true;
                break;
            }
        }
        
        if (is_sensitive && it->is_string()) {
            // 脱敏字符串值
            *it = "[REDACTED]";
        } else if (it->is_object()) {
            // 递归处理嵌套对象
            SanitizeJsonObject(*it);
        } else if (it->is_array()) {
            // 递归处理数组
            for (auto& item : *it) {
                if (item.is_object()) {
                    SanitizeJsonObject(item);
                }
            }
        }
    }
}

} // namespace ai_backend::api::middlewares