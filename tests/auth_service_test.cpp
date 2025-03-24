#include <gtest/gtest.h>
#include "services/auth/auth_service.h"
#include <memory>

namespace ai_backend::test {

class AuthServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        auth_service_ = std::make_shared<services::auth::AuthService>();
    }

    std::shared_ptr<services::auth::AuthService> auth_service_;
};

TEST_F(AuthServiceTest, HashPasswordProducesConsistentResult) {
    std::string password = "TestPassword123";
    std::string salt = "TestSalt";
    
    std::string hash1 = auth_service_->HashPassword(password, salt);
    std::string hash2 = auth_service_->HashPassword(password, salt);
    
    EXPECT_EQ(hash1, hash2);
}

TEST_F(AuthServiceTest, DifferentPasswordsProduceDifferentHashes) {
    std::string password1 = "TestPassword123";
    std::string password2 = "TestPassword124";
    std::string salt = "TestSalt";
    
    std::string hash1 = auth_service_->HashPassword(password1, salt);
    std::string hash2 = auth_service_->HashPassword(password2, salt);
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(AuthServiceTest, DifferentSaltsProduceDifferentHashes) {
    std::string password = "TestPassword123";
    std::string salt1 = "TestSalt1";
    std::string salt2 = "TestSalt2";
    
    std::string hash1 = auth_service_->HashPassword(password, salt1);
    std::string hash2 = auth_service_->HashPassword(password, salt2);
    
    EXPECT_NE(hash1, hash2);
}

} // namespace ai_backend::test
