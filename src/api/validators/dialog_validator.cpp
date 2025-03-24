#include "api/validators/dialog_validator.h"
#include <spdlog/spdlog.h>
#include "services/ai/model_service.h"

namespace ai_backend::api::validators {

bool DialogValidator::ValidateCreateRequest(const nlohmann::json& json, std::string& error_message) {
    // 检查必填字段
    if (!json.contains("title") || !json["title"].is_string()) {
        error_message = "对话标题不能为空";
        return false;
    }
    
    if (!json.contains("model_id") || !json["model_id"].is_string()) {
        error_message = "模型ID不能为空";
        return false;
    }
    
    // 验证字段长度
    std::string title = json["title"].get<std::string>();
    if (title.length() < 1 || title.length() > 128) {
        error_message = "对话标题长度必须在1-128个字符之间";
        return false;
    }
    
    // 验证模型ID是否有效
    std::string model_id = json["model_id"].get<std::string>();
    auto& model_service = services::ai::ModelService::GetInstance();
    auto result = model_service.GetModelInfo(model_id);
    
    if (result.IsError()) {
        error_message = "无效的模型ID";
        return false;
    }
    
    return true;
}

bool DialogValidator::ValidateUpdateRequest(const nlohmann::json& json, std::string& error_message) {
    // 验证可选字段
    if (json.contains("title") && json["title"].is_string()) {
        std::string title = json["title"].get<std::string>();
        if (title.length() < 1 || title.length() > 128) {
            error_message = "对话标题长度必须在1-128个字符之间";
            return false;
        }
    }
    
    // 验证是否有至少一个有效字段
    if (json.empty() || (!json.contains("title") && !json.contains("is_archived"))) {
        error_message = "请提供至少一个要更新的字段";
        return false;
    }
    
    return true;
}

bool DialogValidator::ValidateDialogId(const std::string& dialog_id, std::string& error_message) {
    // 检查ID格式
    const std::regex uuid_regex(
        "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}",
        std::regex_constants::icase
    );
    
    if (!std::regex_match(dialog_id, uuid_regex)) {
        error_message = "无效的对话ID格式";
        return false;
    }
    
    return true;
}

bool DialogValidator::ValidateDialogAccess(const std::string& dialog_id, const std::string& user_id, 
                                         services::dialog::DialogService& dialog_service, 
                                         std::string& error_message) {
    // 验证用户是否有权访问对话
    auto result = dialog_service.ValidateDialogOwnership(dialog_id, user_id);
    
    if (result.IsError()) {
        error_message = result.GetError();
        return false;
    }
    
    return true;
}

} // namespace ai_backend::api::validators