#pragma once

#include <string>
#include <vector>
#include <memory>
#include "core/async/task.h"
#include "common/result.h"
#include "models/dialog.h"

namespace ai_backend::services::dialog {

class DialogService {
public:
    DialogService();
    
    core::async::Task<common::Result<models::Dialog>> GetDialogById(const std::string& dialog_id);
    core::async::Task<common::Result<std::vector<models::Dialog>>> GetDialogsByUserId(const std::string& user_id, int page = 1, int page_size = 20, bool include_archived = false);
    
    core::async::Task<common::Result<models::Dialog>> CreateDialog(const models::Dialog& dialog);
    core::async::Task<common::Result<models::Dialog>> UpdateDialog(const models::Dialog& dialog);
    core::async::Task<common::Result<void>> DeleteDialog(const std::string& dialog_id);
    
    core::async::Task<common::Result<std::string>> ValidateDialogOwnership(const std::string& dialog_id, const std::string& user_id);
};

} // namespace ai_backend::services::dialog