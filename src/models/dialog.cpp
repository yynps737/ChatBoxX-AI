#include "models/dialog.h"
#include <chrono>

namespace ai_backend::models {

Dialog::Dialog()
    : is_archived(false) {
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    created_at = std::ctime(&now_time_t);
    updated_at = created_at;
}

void Dialog::Update() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    updated_at = std::ctime(&now_time_t);
}

nlohmann::json Dialog::ToJson() const {
    nlohmann::json json_obj;
    
    json_obj["id"] = id;
    json_obj["user_id"] = user_id;
    json_obj["title"] = title;
    json_obj["model_id"] = model_id;
    json_obj["is_archived"] = is_archived;
    json_obj["created_at"] = created_at;
    json_obj["updated_at"] = updated_at;
    
    if (!last_message.empty()) {
        json_obj["last_message"] = last_message;
    }
    
    return json_obj;
}

Dialog Dialog::FromJson(const nlohmann::json& json) {
    Dialog dialog;
    
    if (json.contains("id") && json["id"].is_string()) {
        dialog.id = json["id"].get<std::string>();
    }
    
    if (json.contains("user_id") && json["user_id"].is_string()) {
        dialog.user_id = json["user_id"].get<std::string>();
    }
    
    if (json.contains("title") && json["title"].is_string()) {
        dialog.title = json["title"].get<std::string>();
    }
    
    if (json.contains("model_id") && json["model_id"].is_string()) {
        dialog.model_id = json["model_id"].get<std::string>();
    }
    
    if (json.contains("is_archived") && json["is_archived"].is_boolean()) {
        dialog.is_archived = json["is_archived"].get<bool>();
    }
    
    if (json.contains("created_at") && json["created_at"].is_string()) {
        dialog.created_at = json["created_at"].get<std::string>();
    }
    
    if (json.contains("updated_at") && json["updated_at"].is_string()) {
        dialog.updated_at = json["updated_at"].get<std::string>();
    }
    
    if (json.contains("last_message") && json["last_message"].is_string()) {
        dialog.last_message = json["last_message"].get<std::string>();
    }
    
    return dialog;
}

} // namespace ai_backend::models