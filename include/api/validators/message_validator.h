#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace ai_backend::api::validators {

class MessageValidator {
public:
    static bool ValidateCreateRequest(const nlohmann::json& json, std::string& error_message);
    static bool ValidateMessageId(const std::string& message_id, std::string& error_message);
    static bool ValidateDialogId(const std::string& dialog_id, std::string& error_message);
    static bool ValidateStreamParam(const std::string& stream_param, std::string& error_message);
    static bool ValidateRole(const std::string& role, std::string& error_message);
    static bool ValidateTokenLimit(const std::string& content, int max_tokens, std::string& error_message);
};

} // namespace ai_backend::api::validators