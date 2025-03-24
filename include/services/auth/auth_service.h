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
#include <openssl/kdf.h>
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
    try {
        if (token.length() < 10) {
            co_return common::Result<std::string>::Error("无效的令牌格式");
        }
        
        if (!core::utils::JwtHelper::VerifyToken(token, jwt_secret_)) {
            co_return common::Result<std::string>::Error("无效的令牌");
        }
        
        auto payload = core::utils::JwtHelper::GetTokenPayload(token);
        
        if (!payload.contains("sub") || !payload.contains("iss") || !payload.contains("exp") || !payload.contains("iat")) {
            co_return common::Result<std::string>::Error("令牌格式错误");
        }
        
        if (payload["iss"] != jwt_issuer_) {
            co_return common::Result<std::string>::Error("无效的令牌发行者");
        }
        
        if (payload.contains("type") && payload["type"] == "refresh") {
            co_return common::Result<std::string>::Error("刷新令牌不能用于访问资源");
        }
        
        std::string user_id = payload["sub"].get<std::string>();
        
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT is_active FROM users WHERE id = $1",
            user_id
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<std::string>::Error("用户不存在");
        }
        
        bool is_active = result[0][0].as<bool>();
        txn.commit();
        
        db_pool.ReleaseConnection(conn);
        
        if (!is_active) {
            co_return common::Result<std::string>::Error("用户已禁用");
        }
        
        co_return common::Result<std::string>::Ok(user_id);
    } catch (const std::exception& e) {
        spdlog::error("Error validating token: {}", e.what());
        co_return common::Result<std::string>::Error("令牌验证失败");
    }
}

Task<common::Result<std::string>> AuthService::GenerateToken(const std::string& user_id) {
    try {
        nlohmann::json payload;
        payload["sub"] = user_id;
        payload["iss"] = jwt_issuer_;
        payload["iat"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        
        std::string token = core::utils::JwtHelper::CreateToken(payload, jwt_secret_, access_token_lifetime_);
        
        if (token.empty()) {
            co_return common::Result<std::string>::Error("生成令牌失败");
        }
        
        co_return common::Result<std::string>::Ok(token);
    } catch (const std::exception& e) {
        spdlog::error("Error generating token: {}", e.what());
        co_return common::Result<std::string>::Error("生成令牌失败");
    }
}

Task<common::Result<std::string>> AuthService::RefreshToken(const std::string& refresh_token) {
    try {
        if (refresh_token.length() < 10) {
            co_return common::Result<std::string>::Error("无效的刷新令牌格式");
        }
        
        if (!core::utils::JwtHelper::VerifyToken(refresh_token, jwt_secret_)) {
            co_return common::Result<std::string>::Error("无效的刷新令牌");
        }
        
        auto payload = core::utils::JwtHelper::GetTokenPayload(refresh_token);
        
        if (!payload.contains("sub") || !payload.contains("iss") || !payload.contains("exp") || !payload.contains("iat")) {
            co_return common::Result<std::string>::Error("刷新令牌格式错误");
        }
        
        if (payload["iss"] != jwt_issuer_) {
            co_return common::Result<std::string>::Error("无效的令牌发行者");
        }
        
        if (!payload.contains("type") || payload["type"] != "refresh") {
            co_return common::Result<std::string>::Error("无效的刷新令牌类型");
        }
        
        std::string user_id = payload["sub"].get<std::string>();
        
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT is_active FROM users WHERE id = $1",
            user_id
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<std::string>::Error("用户不存在");
        }
        
        bool is_active = result[0][0].as<bool>();
        txn.commit();
        
        db_pool.ReleaseConnection(conn);
        
        if (!is_active) {
            co_return common::Result<std::string>::Error("用户已禁用");
        }
        
        nlohmann::json new_payload;
        new_payload["sub"] = user_id;
        new_payload["iss"] = jwt_issuer_;
        new_payload["iat"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        
        std::string new_token = core::utils::JwtHelper::CreateToken(new_payload, jwt_secret_, access_token_lifetime_);
        
        if (new_token.empty()) {
            co_return common::Result<std::string>::Error("生成新的访问令牌失败");
        }
        
        co_return common::Result<std::string>::Ok(new_token);
    } catch (const std::exception& e) {
        spdlog::error("Error refreshing token: {}", e.what());
        co_return common::Result<std::string>::Error("刷新令牌失败");
    }
}

Task<common::Result<std::pair<std::string, std::string>>> 
AuthService::Login(const std::string& username, const std::string& password) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, password_hash, salt, is_active FROM users WHERE username = $1",
            username
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<std::pair<std::string, std::string>>::Error("用户名或密码错误");
        }
        
        std::string user_id = result[0][0].as<std::string>();
        std::string stored_hash = result[0][1].as<std::string>();
        std::string salt = result[0][2].as<std::string>();
        bool is_active = result[0][3].as<bool>();
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        if (!is_active) {
            co_return common::Result<std::pair<std::string, std::string>>::Error("账号已被禁用");
        }
        
        std::string input_hash = HashPassword(password, salt);
        if (input_hash != stored_hash) {
            co_return common::Result<std::pair<std::string, std::string>>::Error("用户名或密码错误");
        }
        
        nlohmann::json access_payload;
        access_payload["sub"] = user_id;
        access_payload["iss"] = jwt_issuer_;
        access_payload["iat"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        
        std::string access_token = core::utils::JwtHelper::CreateToken(
            access_payload, jwt_secret_, access_token_lifetime_);
        
        if (access_token.empty()) {
            co_return common::Result<std::pair<std::string, std::string>>::Error("生成访问令牌失败");
        }
        
        nlohmann::json refresh_payload;
        refresh_payload["sub"] = user_id;
        refresh_payload["iss"] = jwt_issuer_;
        refresh_payload["iat"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        refresh_payload["type"] = "refresh";
        
        std::string refresh_token = core::utils::JwtHelper::CreateToken(
            refresh_payload, jwt_secret_, refresh_token_lifetime_);
        
        if (refresh_token.empty()) {
            co_return common::Result<std::pair<std::string, std::string>>::Error("生成刷新令牌失败");
        }
        
        try {
            auto conn = co_await db_pool.GetConnectionAsync();
            pqxx::work txn(*conn);
            
            txn.exec_params(
                "UPDATE users SET last_login_at = NOW() WHERE id = $1",
                user_id
            );
            
            txn.commit();
            db_pool.ReleaseConnection(conn);
        } catch (const std::exception& e) {
            spdlog::error("Error updating last login time: {}", e.what());
        }
        
        co_return common::Result<std::pair<std::string, std::string>>::Ok(
            std::make_pair(access_token, refresh_token));
    } catch (const std::exception& e) {
        spdlog::error("Error in login: {}", e.what());
        co_return common::Result<std::pair<std::string, std::string>>::Error("登录失败");
    }
}

Task<common::Result<std::string>> 
AuthService::Register(const std::string& username, const std::string& password, const std::string& email) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        {
            pqxx::work txn(*conn);
            auto result = txn.exec_params(
                "SELECT id FROM users WHERE username = $1",
                username
            );
            
            if (!result.empty()) {
                db_pool.ReleaseConnection(conn);
                co_return common::Result<std::string>::Error("用户名已存在");
            }
            
            txn.commit();
        }
        
        {
            pqxx::work txn(*conn);
            auto result = txn.exec_params(
                "SELECT id FROM users WHERE email = $1",
                email
            );
            
            if (!result.empty()) {
                db_pool.ReleaseConnection(conn);
                co_return common::Result<std::string>::Error("邮箱已被注册");
            }
            
            txn.commit();
        }
        
        std::string salt = core::utils::StringUtils::GenerateRandomString(16);
        std::string password_hash = HashPassword(password, salt);
        std::string user_id = core::utils::UuidGenerator::GenerateUuid();
        
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO users (id, username, email, password_hash, salt, is_active, is_admin, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, true, false, NOW(), NOW())",
            user_id, username, email, password_hash, salt
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::string>::Ok(user_id);
    } catch (const std::exception& e) {
        spdlog::error("Error in registration: {}", e.what());
        co_return common::Result<std::string>::Error("注册失败");
    }
}

} // namespace ai_backend::services::auth