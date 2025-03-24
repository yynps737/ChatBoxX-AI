#pragma once

#include <string>
#include <regex>
#include <nlohmann/json.hpp>
#include "services/dialog/dialog_service.h"

namespace ai_backend::api::validators {

class DialogValidator {
public:
    static bool ValidateCreateRequest(const nlohmann::json& json, std::string& error_message);
    static bool ValidateUpdateRequest(const nlohmann::json& json, std::string& error_message);
    static bool ValidateDialogId(const std::string& dialog_id, std::string& error_message);
    static bool ValidateDialogAccess(const std::string& dialog_id, const std::string& user_id, 
                                    services::dialog::DialogService& dialog_service, 
                                    std::string& error_message);
};

} // namespace ai_backend::api::validators