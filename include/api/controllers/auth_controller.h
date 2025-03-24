#pragma once

#include <memory>
#include <string>

#include "core/http/request.h"
#include "core/http/response.h"
#include "core/async/task.h"
#include "services/auth/auth_service.h"

namespace ai_backend::api::controllers {

class AuthController {
public:
    explicit AuthController(std::shared_ptr<services::auth::AuthService> auth_service);
    
    core::async::Task<core::http::Response> Register(const core::http::Request& request);
    core::async::Task<core::http::Response> Login(const core::http::Request& request);
    core::async::Task<core::http::Response> RefreshToken(const core::http::Request& request);

private:
    std::shared_ptr<services::auth::AuthService> auth_service_;
};

} // namespace ai_backend::api::controllers