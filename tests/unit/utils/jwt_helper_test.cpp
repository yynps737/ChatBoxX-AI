#include "core/utils/jwt_helper.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace ai_backend::tests {

using namespace core::utils;
using json = nlohmann::json;

TEST(JwtHelperTest, CreateAndVerifyToken) {
    std::string secret = "test_secret_key";
    json payload = {
        {"sub", "1234567890"},
        {"name", "Test User"},
        {"admin", true}
    };
    
    // 创建令牌
    std::string token = JwtHelper::CreateToken(payload, secret);
    ASSERT_FALSE(token.empty());
    
    // 验证令牌
    bool valid = JwtHelper::VerifyToken(token, secret);
    EXPECT_TRUE(valid);
    
    // 使用错误密钥验证令牌
    bool invalid = JwtHelper::VerifyToken(token, "wrong_secret");
    EXPECT_FALSE(invalid);
}

TEST(JwtHelperTest, GetTokenPayload) {
    std::string secret = "test_secret_key";
    json original_payload = {
        {"sub", "1234567890"},
        {"name", "Test User"},
        {"admin", true}
    };
    
    // 创建令牌
    std::string token = JwtHelper::CreateToken(original_payload, secret);
    ASSERT_FALSE(token.empty());
    
    // 获取载荷
    json payload = JwtHelper::GetTokenPayload(token);
    EXPECT_EQ(payload["sub"], "1234567890");
    EXPECT_EQ(payload["name"], "Test User");
    EXPECT_EQ(payload["admin"], true);
    
    // 验证过期时间存在
    EXPECT_TRUE(payload.contains("exp"));
    EXPECT_TRUE(payload.contains("iat"));
}

TEST(JwtHelperTest, TokenExpiration) {
    std::string secret = "test_secret_key";
    json payload = {
        {"sub", "1234567890"}
    };
    
    // 创建一个立即过期的令牌
    std::string expired_token = JwtHelper::CreateToken(payload, secret, std::chrono::seconds(0));
    ASSERT_FALSE(expired_token.empty());
    
    // 验证令牌
    bool valid = JwtHelper::VerifyToken(expired_token, secret);
    EXPECT_FALSE(valid);
    
    // 创建一个1小时后过期的令牌
    std::string valid_token = JwtHelper::CreateToken(payload, secret, std::chrono::hours(1));
    ASSERT_FALSE(valid_token.empty());
    
    // 验证令牌
    valid = JwtHelper::VerifyToken(valid_token, secret);
    EXPECT_TRUE(valid);
}

} // namespace ai_backend::tests