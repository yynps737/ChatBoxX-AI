#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <utility>

#include "core/async/task.h"
#include "common/result.h"

namespace ai_backend::services::auth {

class AuthService {
public:
    AuthService();
    
    // 密码哈希函数
    std::string HashPassword(const std::string& password, const std::string& salt);
    
    // 令牌验证
    core::async::Task<common::Result<std::string>> ValidateToken(const std::string& token);
    
    // 生成访问令牌
    core::async::Task<common::Result<std::string>> GenerateToken(const std::string& user_id);
    
    // 刷新令牌
    core::async::Task<common::Result<std::string>> RefreshToken(const std::string& refresh_token);
    
    // 用户登录
    core::async::Task<common::Result<std::pair<std::string, std::string>>> 
    Login(const std::string& username, const std::string& password);
    
    // 用户注册
    core::async::Task<common::Result<std::string>> 
    Register(const std::string& username, const std::string& password, const std::string& email);
    
private:
    // JWT密钥
    std::string jwt_secret_;
    
    // JWT颁发者
    std::string jwt_issuer_;
    
    // 访问令牌有效期
    std::chrono::seconds access_token_lifetime_;
    
    // 刷新令牌有效期
    std::chrono::seconds refresh_token_lifetime_;
};

} // namespace ai_backend::services::auth
