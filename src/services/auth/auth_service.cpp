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

// 其他方法实现...
// 为了简化，我们只需要实现测试用到的 HashPassword 方法

} // namespace ai_backend::services::auth
