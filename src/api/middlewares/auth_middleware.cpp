#include "api/middlewares/auth_middleware.h"
#include <spdlog/spdlog.h>

namespace ai_backend::api::middlewares {

using namespace core::async;

AuthMiddleware::AuthMiddleware(std::shared_ptr<services::auth::AuthService> auth_service)
    : auth_service_(std::move(auth_service)) {
}

Task<bool> AuthMiddleware::Process(core::http::Request& request) {
    // 获取Authorization头
    std::string auth_header = request.GetHeader("Authorization");
    
    // 如果没有Authorization头，继续处理（路由器会检查是否需要认证）
    if (auth_header.empty()) {
        co_return true;
    }
    
    // 验证Authorization头格式
    if (auth_header.substr(0, 7) != "Bearer ") {
        spdlog::warn("Invalid Authorization header format");
        co_return true; // 继续处理，但不设置用户ID
    }
    
    // 提取JWT令牌
    std::string token = auth_header.substr(7);
    
    // 验证令牌
    auto result = co_await auth_service_->ValidateToken(token);
    
    if (result.IsOk()) {
        // 设置用户ID到请求中
        request.user_id = result.GetValue();
        spdlog::debug("Request authenticated for user: {}", result.GetValue());
    } else {
        spdlog::warn("Token validation failed: {}", result.GetError());
        // 继续处理，但不设置用户ID
    }
    
    co_return true;
}

} // namespace ai_backend::api::middlewares