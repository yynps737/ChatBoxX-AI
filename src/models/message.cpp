#include "models/message.h"
#include <chrono>

namespace ai_backend::models {

Message::Message()
    : tokens(0) {
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    created_at = std::ctime(&now_time_t);
}

nlohmann::json Message::ToJson() const {
    nlohmann::json json_obj;
    
    json_obj["id"] = id;
    json_obj["dialog_id"] = dialog_id;
    json_obj["role"] = role;
    json_obj["content"] = content;
    json_obj["type"] = type;
    json_obj["tokens"] = tokens;
    json_obj["created_at"] = created_at;
    
    if (!attachments.empty()) {
        nlohmann::json attachments_json = nlohmann::json::array();
        for (const auto& attachment : attachments) {
            attachments_json.push_back(attachment.ToJson());
        }
        json_obj["attachments"] = attachments_json;
    }
    
    return json_obj;
}

Message Message::FromJson(const nlohmann::json& json) {
    Message message;
    
    if (json.contains("id") && json["id"].is_string()) {
        message.id = json["id"].get<std::string>();
    }
    
    if (json.contains("dialog_id") && json["dialog_id"].is_string()) {
        message.dialog_id = json["dialog_id"].get<std::string>();
    }
    
    if (json.contains("role") && json["role"].is_string()) {
        message.role = json["role"].get<std::string>();
    }
    
    if (json.contains("content") && json["content"].is_string()) {
        message.content = json["content"].get<std::string>();
    }
    
    if (json.contains("type") && json["type"].is_string()) {
        message.type = json["type"].get<std::string>();
    }
    
    if (json.contains("tokens") && json["tokens"].is_number()) {
        message.tokens = json["tokens"].get<size_t>();
    }
    
    if (json.contains("created_at") && json["created_at"].is_string()) {
        message.created_at = json["created_at"].get<std::string>();
    }
    
    if (json.contains("attachments") && json["attachments"].is_array()) {
        for (const auto& attachment_json : json["attachments"]) {
            message.attachments.push_back(Attachment::FromJson(attachment_json));
        }
    }
    
    return message;
}

nlohmann::json Attachment::ToJson() const {
    nlohmann::json json_obj;
    
    json_obj["id"] = id;
    json_obj["type"] = type;
    json_obj["name"] = name;
    json_obj["url"] = url;
    
    return json_obj;
}

Attachment Attachment::FromJson(const nlohmann::json& json) {
    Attachment attachment;
    
    if (json.contains("id") && json["id"].is_string()) {
        attachment.id = json["id"].get<std::string>();
    }
    
    if (json.contains("type") && json["type"].is_string()) {
        attachment.type = json["type"].get<std::string>();
    }
    
    if (json.contains("name") && json["name"].is_string()) {
        attachment.name = json["name"].get<std::string>();
    }
    
    if (json.contains("url") && json["url"].is_string()) {
        attachment.url = json["url"].get<std::string>();
    }
    
    return attachment;
}

} // namespace ai_backend::models