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
    
    // 检查是否使用了默认密钥
    if (jwt_secret_ == "default_secret_key_change_in_production") {
        spdlog::warn("Using default JWT secret key in production is insecure!");
    }
}

std::string AuthService::HashPassword(const std::string& password, const std::string& salt) {
    // 提高PBKDF2迭代次数
    const int iterations = 100000;  // 增加迭代次数，提高安全性
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

Task<common::Result<std::string>> AuthService::ValidateToken(const std::string& token) {
    try {
        if (token.length() < 10) {
            co_return common::Result<std::string>::Error("无效的令牌格式");
        }
        
        // 使用JWT Helper验证令牌
        if (!core::utils::JwtHelper::VerifyToken(token, jwt_secret_)) {
            co_return common::Result<std::string>::Error("无效的令牌");
        }
        
        // 获取payload
        auto payload = core::utils::JwtHelper::GetTokenPayload(token);
        
        // 检查必要的字段
        if (!payload.contains("sub") || !payload.contains("iss") || !payload.contains("exp") || !payload.contains("iat")) {
            co_return common::Result<std::string>::Error("令牌格式错误");
        }
        
        // 验证发行者
        if (payload["iss"] != jwt_issuer_) {
            co_return common::Result<std::string>::Error("无效的令牌发行者");
        }
        
        // 检查令牌类型
        if (payload.contains("type") && payload["type"] == "refresh") {
            co_return common::Result<std::string>::Error("刷新令牌不能用于访问资源");
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
}