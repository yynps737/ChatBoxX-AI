#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace ai_backend::api::validators {

class AuthValidator {
public:
    static bool ValidateUsername(const std::string& username, std::string& error_message);
    static bool ValidatePassword(const std::string& password, std::string& error_message);
    static bool ValidateEmail(const std::string& email, std::string& error_message);
    static bool ValidateToken(const std::string& token, std::string& error_message);
    
    static bool ValidateLoginRequest(const nlohmann::json& json, std::string& error_message);
    static bool ValidateRegisterRequest(const nlohmann::json& json, std::string& error_message);
    static bool ValidateRefreshTokenRequest(const nlohmann::json& json, std::string& error_message);
};

} // namespace ai_backend::api::validators
