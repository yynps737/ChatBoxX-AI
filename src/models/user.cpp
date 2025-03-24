#include "models/user.h"
#include <chrono>

namespace ai_backend::models {

User::User()
    : is_active(true),
      is_admin(false) {
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    created_at = std::ctime(&now_time_t);
    updated_at = created_at;
}

void User::Update() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    updated_at = std::ctime(&now_time_t);
}

nlohmann::json User::ToJson() const {
    nlohmann::json json_obj;
    
    json_obj["id"] = id;
    json_obj["username"] = username;
    json_obj["email"] = email;
    json_obj["is_active"] = is_active;
    json_obj["is_admin"] = is_admin;
    json_obj["created_at"] = created_at;
    json_obj["updated_at"] = updated_at;
    
    // 不包含密码哈希和盐值
    
    return json_obj;
}

nlohmann::json User::ToPublicJson() const {
    nlohmann::json json_obj;
    
    json_obj["id"] = id;
    json_obj["username"] = username;
    json_obj["is_active"] = is_active;
    
    return json_obj;
}

User User::FromJson(const nlohmann::json& json) {
    User user;
    
    if (json.contains("id") && json["id"].is_string()) {
        user.id = json["id"].get<std::string>();
    }
    
    if (json.contains("username") && json["username"].is_string()) {
        user.username = json["username"].get<std::string>();
    }
    
    if (json.contains("email") && json["email"].is_string()) {
        user.email = json["email"].get<std::string>();
    }
    
    if (json.contains("password_hash") && json["password_hash"].is_string()) {
        user.password_hash = json["password_hash"].get<std::string>();
    }
    
    if (json.contains("salt") && json["salt"].is_string()) {
        user.salt = json["salt"].get<std::string>();
    }
    
    if (json.contains("is_active") && json["is_active"].is_boolean()) {
        user.is_active = json["is_active"].get<bool>();
    }
    
    if (json.contains("is_admin") && json["is_admin"].is_boolean()) {
        user.is_admin = json["is_admin"].get<bool>();
    }
    
    if (json.contains("created_at") && json["created_at"].is_string()) {
        user.created_at = json["created_at"].get<std::string>();
    }
    
    if (json.contains("updated_at") && json["updated_at"].is_string()) {
        user.updated_at = json["updated_at"].get<std::string>();
    }
    
    return user;
}

} // namespace ai_backend::models