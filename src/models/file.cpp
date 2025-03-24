#include "models/file.h"
#include <chrono>

namespace ai_backend::models {

File::File()
    : size(0) {
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    created_at = std::ctime(&now_time_t);
}

nlohmann::json File::ToJson() const {
    nlohmann::json json_obj;
    
    json_obj["id"] = id;
    json_obj["user_id"] = user_id;
    json_obj["name"] = name;
    json_obj["type"] = type;
    json_obj["size"] = size;
    json_obj["url"] = url;
    json_obj["created_at"] = created_at;
    
    if (!message_id.empty()) {
        json_obj["message_id"] = message_id;
    }
    
    return json_obj;
}

File File::FromJson(const nlohmann::json& json) {
    File file;
    
    if (json.contains("id") && json["id"].is_string()) {
        file.id = json["id"].get<std::string>();
    }
    
    if (json.contains("user_id") && json["user_id"].is_string()) {
        file.user_id = json["user_id"].get<std::string>();
    }
    
    if (json.contains("name") && json["name"].is_string()) {
        file.name = json["name"].get<std::string>();
    }
    
    if (json.contains("type") && json["type"].is_string()) {
        file.type = json["type"].get<std::string>();
    }
    
    if (json.contains("size") && json["size"].is_number()) {
        file.size = json["size"].get<size_t>();
    }
    
    if (json.contains("url") && json["url"].is_string()) {
        file.url = json["url"].get<std::string>();
    }
    
    if (json.contains("created_at") && json["created_at"].is_string()) {
        file.created_at = json["created_at"].get<std::string>();
    }
    
    if (json.contains("message_id") && json["message_id"].is_string()) {
        file.message_id = json["message_id"].get<std::string>();
    }
    
    return file;
}

} // namespace ai_backend::models