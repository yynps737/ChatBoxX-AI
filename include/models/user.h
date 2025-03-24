#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ai_backend::models {

struct User {
    std::string id;
    std::string username;
    std::string email;
    std::string password_hash;
    std::string salt;
    bool is_active;
    bool is_admin;
    std::string created_at;
    std::string updated_at;
    std::string last_login_at;
    
    User();
    void Update();
    
    nlohmann::json ToJson() const;
    nlohmann::json ToPublicJson() const;
    static User FromJson(const nlohmann::json& json);
};

} // namespace ai_backend::models