#pragma once

#include <memory>
#include <string>

#include "core/http/request.h"
#include "core/http/response.h"
#include "core/async/task.h"
#include "services/dialog/dialog_service.h"

namespace ai_backend::api::controllers {

class DialogController {
public:
    explicit DialogController(std::shared_ptr<services::dialog::DialogService> dialog_service);
    
    core::async::Task<core::http::Response> GetDialogs(const core::http::Request& request);
    core::async::Task<core::http::Response> CreateDialog(const core::http::Request& request);
    core::async::Task<core::http::Response> GetDialogById(const core::http::Request& request);
    core::async::Task<core::http::Response> UpdateDialog(const core::http::Request& request);
    core::async::Task<core::http::Response> DeleteDialog(const core::http::Request& request);

private:
    std::shared_ptr<services::dialog::DialogService> dialog_service_;
};

} // namespace ai_backend::api::controllers