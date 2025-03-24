#include "api/validators/auth_validator.h"
#include <regex>
#include <spdlog/spdlog.h>

namespace ai_backend::api::validators {

bool AuthValidator::ValidateUsername(const std::string& username, std::string& error_message) {
    // 检查用户名长度
    if (username.length() < 3 || username.length() > 20) {
        error_message = "用户名长度必须在3-20个字符之间";
        return false;
    }
    
    // 检查用户名格式
    std::regex username_regex("^[a-zA-Z0-9_]+$");
    if (!std::regex_match(username, username_regex)) {
        error_message = "用户名只能包含字母、数字和下划线";
        return false;
    }
    
    return true;
}

bool AuthValidator::ValidatePassword(const std::string& password, std::string& error_message) {
    // 检查密码长度
    if (password.length() < 8 || password.length() > 32) {
        error_message = "密码长度必须在8-32个字符之间";
        return false;
    }
    
    // 检查密码复杂度
    bool has_uppercase = false;
    bool has_lowercase = false;
    bool has_digit = false;
    bool has_special = false;
    
    for (char c : password) {
        if (std::isupper(c)) has_uppercase = true;
        else if (std::islower(c)) has_lowercase = true;
        else if (std::isdigit(c)) has_digit = true;
        else has_special = true;
    }
    
    if (!(has_uppercase && has_lowercase && has_digit)) {
        error_message = "密码必须包含大写字母、小写字母和数字";
        return false;
    }
    
    return true;
}

bool AuthValidator::ValidateEmail(const std::string& email, std::string& error_message) {
    // 检查邮箱长度
    if (email.length() < 5 || email.length() > 64) {
        error_message = "邮箱长度必须在5-64个字符之间";
        return false;
    }
    
    // 检查邮箱格式
    std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    if (!std::regex_match(email, email_regex)) {
        error_message = "邮箱格式不正确";
        return false;
    }
    
    return true;
}

bool AuthValidator::ValidateToken(const std::string& token, std::string& error_message) {
    // 检查令牌长度
    if (token.empty()) {
        error_message = "令牌不能为空";
        return false;
    }
    
    // 检查令牌格式（简单检查）
    std::regex token_regex(R"([a-zA-Z0-9_\-\.]+\.[a-zA-Z0-9_\-\.]+\.[a-zA-Z0-9_\-\.]+)");
    if (!std::regex_match(token, token_regex)) {
        error_message = "令牌格式不正确";
        return false;
    }
    
    return true;
}

bool AuthValidator::ValidateLoginRequest(const nlohmann::json& json, std::string& error_message) {
    // 检查必填字段
    if (!json.contains("username") || !json["username"].is_string()) {
        error_message = "用户名不能为空";
        return false;
    }
    
    if (!json.contains("password") || !json["password"].is_string()) {
        error_message = "密码不能为空";
        return false;
    }
    
    // 验证字段格式
    std::string username = json["username"].get<std::string>();
    if (!ValidateUsername(username, error_message)) {
        return false;
    }
    
    return true;
}

bool AuthValidator::ValidateRegisterRequest(const nlohmann::json& json, std::string& error_message) {
    // 检查必填字段
    if (!json.contains("username") || !json["username"].is_string()) {
        error_message = "用户名不能为空";
        return false;
    }
    
    if (!json.contains("password") || !json["password"].is_string()) {
        error_message = "密码不能为空";
        return false;
    }
    
    if (!json.contains("email") || !json["email"].is_string()) {
        error_message = "邮箱不能为空";
        return false;
    }
    
    // 验证字段格式
    std::string username = json["username"].get<std::string>();
    if (!ValidateUsername(username, error_message)) {
        return false;
    }
    
    std::string password = json["password"].get<std::string>();
    if (!ValidatePassword(password, error_message)) {
        return false;
    }
    
    std::string email = json["email"].get<std::string>();
    if (!ValidateEmail(email, error_message)) {
        return false;
    }
    
    return true;
}

bool AuthValidator::ValidateRefreshTokenRequest(const nlohmann::json& json, std::string& error_message) {
    // 检查必填字段
    if (!json.contains("refresh_token") || !json["refresh_token"].is_string()) {
        error_message = "刷新令牌不能为空";
        return false;
    }
    
    // 验证令牌格式
    std::string refresh_token = json["refresh_token"].get<std::string>();
    if (!ValidateToken(refresh_token, error_message)) {
        return false;
    }
    
    return true;
}

} // namespace ai_backend::api::validators