#pragma once

#include <string>
#include <vector>
#include <memory>
#include "core/async/task.h"
#include "common/result.h"
#include "models/message.h"

namespace ai_backend::services::message {

class MessageService {
public:
    MessageService();
    
    core::async::Task<common::Result<models::Message>> GetMessageById(const std::string& message_id);
    core::async::Task<common::Result<std::vector<models::Message>>> GetMessagesByDialogId(const std::string& dialog_id, int page = 1, int page_size = 20);
    core::async::Task<common::Result<std::vector<models::Message>>> GetAllMessagesByDialogId(const std::string& dialog_id);
    
    core::async::Task<common::Result<models::Message>> CreateMessage(const models::Message& message);
    core::async::Task<common::Result<void>> DeleteMessage(const std::string& message_id);
    
    core::async::Task<common::Result<int>> CountTokens(const std::string& content);
};

} // namespace ai_backend::services::message