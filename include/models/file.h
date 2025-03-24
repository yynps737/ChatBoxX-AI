#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ai_backend::models {

struct File {
    std::string id;
    std::string user_id;
    std::string message_id;
    std::string name;
    std::string type;
    size_t size;
    std::string url;
    std::string created_at;
    
    File();
    
    nlohmann::json ToJson() const;
    static File FromJson(const nlohmann::json& json);
};

} // namespace ai_backend::models