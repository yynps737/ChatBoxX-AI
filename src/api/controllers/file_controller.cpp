#include "api/controllers/file_controller.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai_backend::api::controllers {

using json = nlohmann::json;
using namespace core::async;
using namespace core::http;

FileController::FileController(std::shared_ptr<services::file::FileService> file_service)
    : file_service_(std::move(file_service)) {
}

Task<Response> FileController::UploadFile(const Request& request) {
    // 实现代码...
    co_return Response::Ok();
}

Task<Response> FileController::GetFile(const Request& request) {
    // 实现代码...
    co_return Response::Ok();
}

Task<Response> FileController::DeleteFile(const Request& request) {
    // 实现代码...
    co_return Response::Ok();
}

} // namespace ai_backend::api::controllers
