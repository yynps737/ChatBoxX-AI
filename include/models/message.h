#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ai_backend::models {

struct Attachment {
    std::string id;
    std::string type;
    std::string name;
    std::string url;
    
    nlohmann::json ToJson() const;
    static Attachment FromJson(const nlohmann::json& json);
};

struct Message {
    std::string id;
    std::string dialog_id;
    std::string role;
    std::string content;
    std::string type;
    size_t tokens;
    std::string created_at;
    std::vector<Attachment> attachments;
    
    Message();
    
    nlohmann::json ToJson() const;
    static Message FromJson(const nlohmann::json& json);
};

} // namespace ai_backend::models