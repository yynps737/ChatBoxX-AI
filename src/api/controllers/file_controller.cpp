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
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 检查请求头的Content-Type
        auto content_type = request.GetHeader("Content-Type");
        if (content_type.find("multipart/form-data") == std::string::npos) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", "请求必须使用multipart/form-data格式"},
                {"data", nullptr}
            });
        }
        
        // 解析multipart表单数据
        auto parse_result = co_await file_service_->ParseMultipartFormData(request);
        if (parse_result.IsError()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", parse_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取上传的文件和元数据
        const auto& form_data = parse_result.GetValue();
        
        // 检查是否有文件
        if (form_data.files.empty()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", "没有上传文件"},
                {"data", nullptr}
            });
        }
        
        // 获取消息ID(可选)
        std::optional<std::string> message_id;
        auto it = form_data.fields.find("message_id");
        if (it != form_data.fields.end()) {
            message_id = it->second;
        }
        
        // 保存文件
        std::vector<models::File> saved_files;
        for (const auto& file_data : form_data.files) {
            models::File file;
            file.user_id = request.user_id.value();
            file.name = file_data.filename;
            file.type = file_data.content_type;
            file.size = file_data.data.size();
            
            if (message_id.has_value()) {
                file.message_id = message_id.value();
            }
            
            auto save_result = co_await file_service_->SaveFile(file, file_data.data);
            if (save_result.IsError()) {
                spdlog::error("Failed to save file {}: {}", file.name, save_result.GetError());
                continue;
            }
            
            saved_files.push_back(save_result.GetValue());
        }
        
        if (saved_files.empty()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", "保存文件失败"},
                {"data", nullptr}
            });
        }
        
        // 转换结果为JSON
        json files_json = json::array();
        for (const auto& file : saved_files) {
            files_json.push_back({
                {"id", file.id},
                {"name", file.name},
                {"type", file.type},
                {"size", file.size},
                {"created_at", file.created_at},
                {"url", file.url}
            });
        }
        
        // 返回成功响应
        co_return Response::Created({
            {"code", 0},
            {"message", "上传成功"},
            {"data", {
                {"files", files_json}
            }}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in UploadFile: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> FileController::GetFile(const Request& request) {
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 获取文件ID
        std::string file_id = request.GetPathParam("id");
        
        // 获取文件
        auto result = co_await file_service_->GetFileById(file_id);
        
        if (result.IsError()) {
            co_return Response::NotFound({
                {"code", 404},
                {"message", "文件不存在"},
                {"data", nullptr}
            });
        }
        
        const auto& file = result.GetValue();
        
        // 验证用户是否有权访问此文件
        if (file.user_id != request.user_id.value()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", "没有权限访问此文件"},
                {"data", nullptr}
            });
        }
        
        // 检查是否请求下载
        bool download = request.GetQueryParam("download", "false") == "true";
        
        if (download) {
            // 获取文件内容
            auto content_result = co_await file_service_->GetFileContent(file_id);
            
            if (content_result.IsError()) {
                co_return Response::InternalServerError({
                    {"code", 500},
                    {"message", content_result.GetError()},
                    {"data", nullptr}
                });
            }
            
            // 构建文件下载响应
            Response response;
            response.status_code = 200;
            response.body = std::string(content_result.GetValue().begin(), content_result.GetValue().end());
            response.headers["Content-Type"] = file.type;
            response.headers["Content-Disposition"] = "attachment; filename=\"" + file.name + "\"";
            response.headers["Content-Length"] = std::to_string(file.size);
            
            co_return response;
        } else {
            // 返回文件元数据
            co_return Response::OK({
                {"code", 0},
                {"message", "获取成功"},
                {"data", {
                    {"id", file.id},
                    {"name", file.name},
                    {"type", file.type},
                    {"size", file.size},
                    {"created_at", file.created_at},
                    {"url", file.url}
                }}
            });
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFile: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> FileController::DeleteFile(const Request& request) {
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 获取文件ID
        std::string file_id = request.GetPathParam("id");
        
        // 获取文件
        auto result = co_await file_service_->GetFileById(file_id);
        
        if (result.IsError()) {
            co_return Response::NotFound({
                {"code", 404},
                {"message", "文件不存在"},
                {"data", nullptr}
            });
        }
        
        const auto& file = result.GetValue();
        
        // 验证用户是否有权删除此文件
        if (file.user_id != request.user_id.value()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", "没有权限删除此文件"},
                {"data", nullptr}
            });
        }
        
        // 删除文件
        auto delete_result = co_await file_service_->DeleteFile(file_id);
        
        if (delete_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", delete_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "删除成功"},
            {"data", nullptr}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in DeleteFile: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

} // namespace ai_backend::api::controllers