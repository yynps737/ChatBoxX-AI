#pragma once

#include <string>
#include <memory>
#include "core/http/request.h"
#include "core/async/task.h"
#include "services/auth/auth_service.h"

namespace ai_backend::api::middlewares {

// 认证中间件，处理请求认证
class AuthMiddleware {
public:
    explicit AuthMiddleware(std::shared_ptr<services::auth::AuthService> auth_service);
    
    // 处理请求，返回是否继续处理
    core::async::Task<bool> Process(core::http::Request& request);

private:
    std::shared_ptr<services::auth::AuthService> auth_service_;
};

} // namespace ai_backend::api::middlewares