#include "api/validators/message_validator.h"
#include <regex>
#include <spdlog/spdlog.h>

namespace ai_backend::api::validators {

bool MessageValidator::ValidateCreateRequest(const nlohmann::json& json, std::string& error_message) {
    // 检查必填字段
    if (!json.contains("content") || !json["content"].is_string()) {
        error_message = "消息内容不能为空";
        return false;
    }
    
    // 验证内容长度
    std::string content = json["content"].get<std::string>();
    if (content.empty()) {
        error_message = "消息内容不能为空";
        return false;
    }
    
    if (content.length() > 10000) {
        error_message = "消息内容不能超过10000个字符";
        return false;
    }
    
    // 验证类型
    if (json.contains("type") && json["type"].is_string()) {
        std::string type = json["type"].get<std::string>();
        if (type != "text" && type != "image" && type != "file" && type != "code" && type != "system") {
            error_message = "无效的消息类型";
            return false;
        }
    }
    
    // 验证附件
    if (json.contains("attachments") && json["attachments"].is_array()) {
        for (const auto& attachment : json["attachments"]) {
            if (!attachment.contains("id") || !attachment["id"].is_string() ||
                !attachment.contains("type") || !attachment["type"].is_string()) {
                error_message = "附件必须包含id和type字段";
                return false;
            }
            
            std::string type = attachment["type"].get<std::string>();
            if (type != "image" && type != "file" && type != "code") {
                error_message = "无效的附件类型";
                return false;
            }
        }
    }
    
    return true;
}

bool MessageValidator::ValidateMessageId(const std::string& message_id, std::string& error_message) {
    // 检查ID格式
    const std::regex uuid_regex(
        "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}",
        std::regex_constants::icase
    );
    
    if (!std::regex_match(message_id, uuid_regex)) {
        error_message = "无效的消息ID格式";
        return false;
    }
    
    return true;
}

bool MessageValidator::ValidateDialogId(const std::string& dialog_id, std::string& error_message) {
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

bool MessageValidator::ValidateStreamParam(const std::string& stream_param, std::string& error_message) {
    // 验证stream参数
    if (stream_param != "true" && stream_param != "false") {
        error_message = "stream参数必须为true或false";
        return false;
    }
    
    return true;
}

bool MessageValidator::ValidateRole(const std::string& role, std::string& error_message) {
    // 验证消息角色
    if (role != "user" && role != "assistant" && role != "system") {
        error_message = "无效的消息角色";
        return false;
    }
    
    return true;
}

bool MessageValidator::ValidateTokenLimit(const std::string& content, int max_tokens, std::string& error_message) {
    // 简单估算token数量（每个中文字符约4个字节，每个英文单词约1.3个token）
    int estimated_tokens = 0;
    bool in_word = false;
    
    for (char c : content) {
        if (static_cast<unsigned char>(c) > 127) {
            // 中文或其他非ASCII字符
            estimated_tokens += 2;
            in_word = false;
        } else if (std::isalnum(c)) {
            // 字母或数字
            if (!in_word) {
                estimated_tokens += 1;
                in_word = true;
            }
        } else {
            // 标点符号或空格
            in_word = false;
            if (!std::isspace(c)) {
                estimated_tokens += 1;
            }
        }
    }
    
    if (estimated_tokens > max_tokens) {
        error_message = "消息内容超过最大token限制";
        return false;
    }
    
    return true;
}

} // namespace ai_backend::api::validators