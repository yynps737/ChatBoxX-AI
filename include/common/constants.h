#pragma once

#include <string>
#include <vector>

namespace ai_backend::common::constants {

// API相关常量
extern const std::string API_VERSION;
extern const std::string API_PREFIX;

// 认证相关常量
extern const int ACCESS_TOKEN_LIFETIME_SECONDS;
extern const int REFRESH_TOKEN_LIFETIME_SECONDS;
extern const std::string AUTH_HEADER;
extern const std::string AUTH_SCHEME;

// 分页相关常量
extern const int DEFAULT_PAGE_SIZE;
extern const int MAX_PAGE_SIZE;

// 文件相关常量
extern const size_t MAX_FILE_SIZE;
extern const std::vector<std::string> ALLOWED_IMAGE_TYPES;
extern const std::vector<std::string> ALLOWED_DOCUMENT_TYPES;

// 系统相关常量
extern const int DEFAULT_HTTP_PORT;
extern const int DEFAULT_HTTPS_PORT;
extern const int CONNECTION_TIMEOUT_SECONDS;
extern const int DATABASE_CONNECTION_POOL_MIN;
extern const int DATABASE_CONNECTION_POOL_MAX;

// 模型相关常量
extern const int DEFAULT_MAX_TOKENS;
extern const double DEFAULT_TEMPERATURE;
extern const double DEFAULT_TOP_P;
extern const int CONTEXT_WINDOW_SIZE;

// 速率限制常量
extern const int DEFAULT_RATE_LIMIT_PER_MINUTE;
extern const int DEFAULT_RATE_LIMIT_PER_HOUR;

// 日志相关常量
extern const std::string DEFAULT_LOG_LEVEL;

} // namespace ai_backend::common::constants