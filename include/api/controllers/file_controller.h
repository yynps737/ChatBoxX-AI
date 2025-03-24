#pragma once

#include <memory>
#include <string>

#include "core/http/request.h"
#include "core/http/response.h"
#include "core/async/task.h"
#include "services/file/file_service.h"

namespace ai_backend::api::controllers {

class FileController {
public:
    explicit FileController(std::shared_ptr<services::file::FileService> file_service);
    
    core::async::Task<core::http::Response> UploadFile(const core::http::Request& request);
    core::async::Task<core::http::Response> GetFile(const core::http::Request& request);
    core::async::Task<core::http::Response> DeleteFile(const core::http::Request& request);

private:
    std::shared_ptr<services::file::FileService> file_service_;
};

} // namespace ai_backend::api::controllers