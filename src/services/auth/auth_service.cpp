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
        config.GetInt("auth.access_token_lifetime", 3600)  // 默认1小时
    );
    refresh_token_lifetime_ = std::chrono::seconds(
        config.GetInt("auth.refresh_token_lifetime", 2592000)  // 默认30天
    );
}

Task<common::Result<std::string>> AuthService::ValidateToken(const std::string& token) {
    try {
        // 使用JWT Helper验证令牌
        if (!core::utils::JwtHelper::VerifyToken(token, jwt_secret_)) {
            co_return common::Result<std::string>::Error("无效的令牌");
        }
        
        // 获取payload
        auto payload = core::utils::JwtHelper::GetTokenPayload(token);
        
        // 检查必要的字段
        if (!payload.contains("sub") || !payload.contains("iss")) {
            co_return common::Result<std::string>::Error("令牌格式错误");
        }
        
        // 验证发行者
        if (payload["iss"] != jwt_issuer_) {
            co_return common::Result<std::string>::Error("无效的令牌发行者");
        }
        
        // 返回用户ID
        std::string user_id = payload["sub"].get<std::string>();
        
        // 验证用户是否存在和活跃
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
        payload["type"] = "access";
        
        std::string token = core::utils::JwtHelper::CreateToken(
            payload, jwt_secret_, access_token_lifetime_
        );
        
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
        // 验证刷新令牌
        if (!core::utils::JwtHelper::VerifyToken(refresh_token, jwt_secret_)) {
            co_return common::Result<std::string>::Error("无效的刷新令牌");
        }
        
        // 获取payload
        auto payload = core::utils::JwtHelper::GetTokenPayload(refresh_token);
        
        // 检查必要的字段
        if (!payload.contains("sub") || !payload.contains("iss") || !payload.contains("type")) {
            co_return common::Result<std::string>::Error("令牌格式错误");
        }
        
        // 验证发行者
        if (payload["iss"] != jwt_issuer_) {
            co_return common::Result<std::string>::Error("无效的令牌发行者");
        }
        
        // 验证类型
        if (payload["type"] != "refresh") {
            co_return common::Result<std::string>::Error("不是有效的刷新令牌");
        }
        
        // 获取用户ID
        std::string user_id = payload["sub"].get<std::string>();
        
        // 验证用户是否存在和活跃
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
        
        // 生成新的访问令牌
        co_return co_await GenerateToken(user_id);
    } catch (const std::exception& e) {
        spdlog::error("Error refreshing token: {}", e.what());
        co_return common::Result<std::string>::Error("刷新令牌失败");
    }
}

Task<common::Result<std::pair<std::string, std::string>>>
AuthService::Login(const std::string& username, const std::string& password) {
    try {
        // 获取数据库连接
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        // 查询用户
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
        
        // 验证密码
        std::string hashed_password = HashPassword(password, salt);
        
        if (hashed_password != stored_hash) {
            co_return common::Result<std::pair<std::string, std::string>>::Error("用户名或密码错误");
        }
        
        // 生成访问令牌
        auto access_token_result = co_await GenerateToken(user_id);
        
        if (access_token_result.IsError()) {
            co_return common::Result<std::pair<std::string, std::string>>::Error(
                access_token_result.GetError()
            );
        }
        
        // 生成刷新令牌
        nlohmann::json refresh_payload;
        refresh_payload["sub"] = user_id;
        refresh_payload["iss"] = jwt_issuer_;
        refresh_payload["type"] = "refresh";
        
        std::string refresh_token = core::utils::JwtHelper::CreateToken(
            refresh_payload, jwt_secret_, refresh_token_lifetime_
        );
        
        if (refresh_token.empty()) {
            co_return common::Result<std::pair<std::string, std::string>>::Error("生成刷新令牌失败");
        }
        
        // 返回令牌对
        std::pair<std::string, std::string> tokens(
            access_token_result.GetValue(),
            refresh_token
        );
        
        co_return common::Result<std::pair<std::string, std::string>>::Ok(tokens);
    } catch (const std::exception& e) {
        spdlog::error("Error in login: {}", e.what());
        co_return common::Result<std::pair<std::string, std::string>>::Error("登录失败");
    }
}

Task<common::Result<std::string>>
AuthService::Register(const std::string& username, const std::string& password, const std::string& email) {
    try {
        // 获取数据库连接
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        // 检查用户名是否已存在
        pqxx::work txn1(*conn);
        auto result1 = txn1.exec_params(
            "SELECT id FROM users WHERE username = $1",
            username
        );
        
        if (!result1.empty()) {
            txn1.abort();
            db_pool.ReleaseConnection(conn);
            co_return common::Result<std::string>::Error("用户名已存在");
        }
        
        // 检查邮箱是否已存在
        auto result2 = txn1.exec_params(
            "SELECT id FROM users WHERE email = $1",
            email
        );
        
        if (!result2.empty()) {
            txn1.abort();
            db_pool.ReleaseConnection(conn);
            co_return common::Result<std::string>::Error("邮箱已被注册");
        }
        
        txn1.commit();
        
        // 生成用户ID
        std::string user_id = core::utils::UuidGenerator::GenerateUuid();
        
        // 生成盐值
        std::string salt = GenerateSalt();
        
        // 哈希密码
        std::string password_hash = HashPassword(password, salt);
        
        // 插入用户记录
        pqxx::work txn2(*conn);
        txn2.exec_params(
            "INSERT INTO users (id, username, email, password_hash, salt, is_active, is_admin) "
            "VALUES ($1, $2, $3, $4, $5, true, false)",
            user_id, username, email, password_hash, salt
        );
        
        txn2.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::string>::Ok(user_id);
    } catch (const std::exception& e) {
        spdlog::error("Error in register: {}", e.what());
        co_return common::Result<std::string>::Error("注册失败");
    }
}

std::string AuthService::HashPassword(const std::string& password, const std::string& salt) {
    // 使用PBKDF2进行密码哈希
    const int iterations = 10000;  // 迭代次数
    const int key_length = 32;     // 输出长度，以字节为单位
    
    unsigned char out[key_length];
    
    // 组合密码和盐值
    std::string salted_password = password + salt;
    
    // 使用PBKDF2-HMAC-SHA256进行密码哈希
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
    
    // 转换为十六进制字符串
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < key_length; i++) {
        ss << std::setw(2) << static_cast<int>(out[i]);
    }
    
    return ss.str();
}

std::string AuthService::GenerateSalt() {
    // 生成32字节的随机盐值
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::vector<unsigned char> salt_bytes(32);
    for (auto& byte : salt_bytes) {
        byte = static_cast<unsigned char>(dis(gen));
    }
    
    // 转换为十六进制字符串
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto byte : salt_bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    
    return ss.str();
}

} // namespace ai_backend::services::auth