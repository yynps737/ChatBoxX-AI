#include "services/auth/auth_service.h"
#include "core/utils/jwt_helper.h"
#include "core/utils/string_utils.h"
#include "core/utils/uuid.h"
#include "core/config/config_manager.h"
#include "core/db/connection_pool.h"
#include <spdlog/spdlog.h>
#include <pqxx/pqxx>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <chrono>
#include <iomanip>
#include <random>

namespace ai_backend::services::auth {

using namespace core::async;

AuthService::AuthService() {
    auto& config = core::config::ConfigManager::GetInstance();
    
    jwt_secret_ = config.GetString("auth.jwt_secret", "default_secret_key_change_in_production");
    jwt_issuer_ = config.GetString("auth.jwt_issuer", "ai_backend");
    access_token_lifetime_ = std::chrono::seconds(
        config.GetInt("auth.access_token_lifetime", 3600)
    );
    refresh_token_lifetime_ = std::chrono::seconds(
        config.GetInt("auth.refresh_token_lifetime", 2592000)
    );
    
    if (jwt_secret_ == "default_secret_key_change_in_production") {
        spdlog::warn("Using default JWT secret key in production is insecure!");
    }
}

std::string AuthService::HashPassword(const std::string& password, const std::string& salt) {
    const int iterations = 100000;
    const int key_length = 32;
    
    unsigned char out[key_length];
    
    std::string salted_password = password + salt;
    
    PKCS5_PBKDF2_HMAC(
        salted_password.c_str(), 
        salted_password.length(),
        reinterpret_cast<const unsigned char*>(salt.c_str()),
        salt.length(),
        iterations,
        EVP_sha256(),
        key_length,
        out
    );
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < key_length; i++) {
        ss << std::setw(2) << static_cast<int>(out[i]);
    }
    
    return ss.str();
}

Task<common::Result<std::string>> AuthService::ValidateToken(const std::string& token) {
    // 验证Token有效性
    if (!core::utils::JwtHelper::VerifyToken(token, jwt_secret_)) {
        co_return common::Result<std::string>::Error("无效的令牌或令牌已过期");
    }
    
    // 解析Token获取用户ID
    auto payload = core::utils::JwtHelper::GetTokenPayload(token);
    if (!payload.contains("sub")) {
        co_return common::Result<std::string>::Error("令牌缺少用户ID");
    }
    
    co_return common::Result<std::string>::Ok(payload["sub"].get<std::string>());
}

Task<common::Result<std::string>> AuthService::GenerateToken(const std::string& user_id) {
    // 创建Token
    nlohmann::json payload;
    payload["sub"] = user_id;
    payload["iss"] = jwt_issuer_;
    
    std::string token = core::utils::JwtHelper::CreateToken(
        payload, jwt_secret_, access_token_lifetime_);
    
    if (token.empty()) {
        co_return common::Result<std::string>::Error("生成令牌失败");
    }
    
    co_return common::Result<std::string>::Ok(token);
}

Task<common::Result<std::string>> AuthService::RefreshToken(const std::string& refresh_token) {
    // 简易实现：验证刷新令牌并生成新的访问令牌
    auto result = co_await ValidateToken(refresh_token);
    
    if (result.IsError()) {
        co_return result;
    }
    
    // 生成新的访问令牌
    co_return co_await GenerateToken(result.GetValue());
}

Task<common::Result<std::pair<std::string, std::string>>> 
AuthService::Login(const std::string& username, const std::string& password) {
    try {
        // 简易实现：返回错误，表示未实现
        co_return common::Result<std::pair<std::string, std::string>>::Error("未实现的方法");
    } catch (const std::exception& e) {
        co_return common::Result<std::pair<std::string, std::string>>::Error(e.what());
    }
}

Task<common::Result<std::string>> 
AuthService::Register(const std::string& username, const std::string& password, const std::string& email) {
    try {
        // 简易实现：返回错误，表示未实现
        co_return common::Result<std::string>::Error("未实现的方法");
    } catch (const std::exception& e) {
        co_return common::Result<std::string>::Error(e.what());
    }
}

} // namespace ai_backend::services::auth
