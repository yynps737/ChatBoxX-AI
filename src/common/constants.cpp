#include "common/constants.h"

namespace ai_backend::common::constants {

// API相关常量
const std::string API_VERSION = "v1";
const std::string API_PREFIX = "/api/" + API_VERSION;

// 认证相关常量
const int ACCESS_TOKEN_LIFETIME_SECONDS = 3600;          // 1小时
const int REFRESH_TOKEN_LIFETIME_SECONDS = 2592000;      // 30天
const std::string AUTH_HEADER = "Authorization";
const std::string AUTH_SCHEME = "Bearer";

// 分页相关常量
const int DEFAULT_PAGE_SIZE = 20;
const int MAX_PAGE_SIZE = 100;

// 文件相关常量
const size_t MAX_FILE_SIZE = 10 * 1024 * 1024;          // 10MB
const std::vector<std::string> ALLOWED_IMAGE_TYPES = {
    "image/jpeg", "image/png", "image/gif", "image/webp"
};
const std::vector<std::string> ALLOWED_DOCUMENT_TYPES = {
    "application/pdf", "application/msword",
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    "text/plain", "text/markdown", "text/csv"
};

// 系统相关常量
const int DEFAULT_HTTP_PORT = 8080;
const int DEFAULT_HTTPS_PORT = 8443;
const int CONNECTION_TIMEOUT_SECONDS = 30;
const int DATABASE_CONNECTION_POOL_MIN = 5;
const int DATABASE_CONNECTION_POOL_MAX = 20;

// 模型相关常量
const int DEFAULT_MAX_TOKENS = 2048;
const double DEFAULT_TEMPERATURE = 0.7;
const double DEFAULT_TOP_P = 0.9;
const int CONTEXT_WINDOW_SIZE = 16384;

// 速率限制常量
const int DEFAULT_RATE_LIMIT_PER_MINUTE = 60;
const int DEFAULT_RATE_LIMIT_PER_HOUR = 1000;

// 日志相关常量
const std::string DEFAULT_LOG_LEVEL = "info";

} // namespace ai_backend::common::constants