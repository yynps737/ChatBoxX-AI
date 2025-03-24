#pragma once

#include <string>
#include <optional>
#include "core/async/task.h"
#include "common/result.h"

namespace ai_backend::services::auth {

// 认证服务类
class AuthService {
public:
    AuthService();
    
    // 验证令牌
    core::async::Task<common::Result<std::string>> ValidateToken(const std::string& token);
    
    // 生成令牌
    core::async::Task<common::Result<std::string>> GenerateToken(const std::string& user_id);
    
    // 刷新令牌
    core::async::Task<common::Result<std::string>> RefreshToken(const std::string& refresh_token);
    
    // 登录
    core::async::Task<common::Result<std::pair<std::string, std::string>>>
    Login(const std::string& username, const std::string& password);
    
    // 注册
    core::async::Task<common::Result<std::string>>
    Register(const std::string& username, const std::string& password, const std::string& email);

private:
    std::string jwt_secret_;
    std::string jwt_issuer_;
    std::chrono::seconds access_token_lifetime_;
    std::chrono::seconds refresh_token_lifetime_;
};

} // namespace ai_backend::services::auth