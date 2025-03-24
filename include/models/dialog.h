#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ai_backend::models {

struct Dialog {
    std::string id;
    std::string user_id;
    std::string title;
    std::string model_id;
    bool is_archived;
    std::string created_at;
    std::string updated_at;
    std::string last_message;
    
    Dialog();
    void Update();
    
    nlohmann::json ToJson() const;
    static Dialog FromJson(const nlohmann::json& json);
};

} // namespace ai_backend::models