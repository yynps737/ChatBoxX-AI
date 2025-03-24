#include "api/controllers/dialog_controller.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai_backend::api::controllers {

using json = nlohmann::json;
using namespace core::async;
using namespace core::http;

DialogController::DialogController(std::shared_ptr<services::dialog::DialogService> dialog_service)
    : dialog_service_(std::move(dialog_service)) {
}

Task<Response> DialogController::GetDialogs(const Request& request) {
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 获取分页参数
        int page = std::stoi(request.GetQueryParam("page", "1"));
        int page_size = std::stoi(request.GetQueryParam("page_size", "20"));
        bool include_archived = request.GetQueryParam("include_archived", "false") == "true";
        
        // 获取对话列表
        auto result = co_await dialog_service_->GetDialogsByUserId(
            request.user_id.value(), 
            page, 
            page_size, 
            include_archived
        );
        
        if (result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 转换结果为JSON
        json dialogs_json = json::array();
        for (const auto& dialog : result.GetValue()) {
            dialogs_json.push_back({
                {"id", dialog.id},
                {"title", dialog.title},
                {"model_id", dialog.model_id},
                {"created_at", dialog.created_at},
                {"updated_at", dialog.updated_at},
                {"is_archived", dialog.is_archived},
                {"last_message", dialog.last_message}
            });
        }
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "获取成功"},
            {"data", {
                {"dialogs", dialogs_json},
                {"total", dialogs_json.size()},
                {"page", page},
                {"page_size", page_size}
            }}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetDialogs: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> DialogController::CreateDialog(const Request& request) {
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 解析请求体JSON
        json request_body = json::parse(request.body);
        
        // 验证请求体
        if (!request_body.contains("title") || !request_body["title"].is_string() ||
            !request_body.contains("model_id") || !request_body["model_id"].is_string()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", "标题和模型ID不能为空"},
                {"data", nullptr}
            });
        }
        
        // 创建对话对象
        models::Dialog dialog;
        dialog.user_id = request.user_id.value();
        dialog.title = request_body["title"].get<std::string>();
        dialog.model_id = request_body["model_id"].get<std::string>();
        
        // 调用服务创建对话
        auto result = co_await dialog_service_->CreateDialog(dialog);
        
        if (result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取创建的对话
        const auto& created_dialog = result.GetValue();
        
        // 转换为JSON
        json dialog_json = {
            {"id", created_dialog.id},
            {"title", created_dialog.title},
            {"model_id", created_dialog.model_id},
            {"created_at", created_dialog.created_at},
            {"updated_at", created_dialog.updated_at},
            {"is_archived", created_dialog.is_archived}
        };
        
        // 返回成功响应
        co_return Response::Created({
            {"code", 0},
            {"message", "创建成功"},
            {"data", dialog_json}
        });
        
    } catch (const json::exception& e) {
        spdlog::error("JSON error in CreateDialog: {}", e.what());
        co_return Response::BadRequest({
            {"code", 400},
            {"message", "请求格式错误"},
            {"data", nullptr}
        });
    } catch (const std::exception& e) {
        spdlog::error("Error in CreateDialog: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> DialogController::GetDialogById(const Request& request) {
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 获取对话ID
        std::string dialog_id = request.GetPathParam("id");
        
        // 验证用户是否有权访问此对话
        auto validation = co_await dialog_service_->ValidateDialogOwnership(dialog_id, request.user_id.value());
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取对话详情
        auto result = co_await dialog_service_->GetDialogById(dialog_id);
        
        if (result.IsError()) {
            co_return Response::NotFound({
                {"code", 404},
                {"message", "对话不存在"},
                {"data", nullptr}
            });
        }
        
        // 获取对话
        const auto& dialog = result.GetValue();
        
        // 转换为JSON
        json dialog_json = {
            {"id", dialog.id},
            {"title", dialog.title},
            {"model_id", dialog.model_id},
            {"created_at", dialog.created_at},
            {"updated_at", dialog.updated_at},
            {"is_archived", dialog.is_archived}
        };
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "获取成功"},
            {"data", dialog_json}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetDialogById: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> DialogController::UpdateDialog(const Request& request) {
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 获取对话ID
        std::string dialog_id = request.GetPathParam("id");
        
        // 验证用户是否有权访问此对话
        auto validation = co_await dialog_service_->ValidateDialogOwnership(dialog_id, request.user_id.value());
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 解析请求体JSON
        json request_body = json::parse(request.body);
        
        // 获取当前对话
        auto dialog_result = co_await dialog_service_->GetDialogById(dialog_id);
        if (dialog_result.IsError()) {
            co_return Response::NotFound({
                {"code", 404},
                {"message", "对话不存在"},
                {"data", nullptr}
            });
        }
        
        auto dialog = dialog_result.GetValue();
        
        // 更新对话信息
        if (request_body.contains("title") && request_body["title"].is_string()) {
            dialog.title = request_body["title"].get<std::string>();
        }
        
        if (request_body.contains("is_archived") && request_body["is_archived"].is_boolean()) {
            dialog.is_archived = request_body["is_archived"].get<bool>();
        }
        
        // 调用服务更新对话
        auto result = co_await dialog_service_->UpdateDialog(dialog);
        
        if (result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取更新后的对话
        const auto& updated_dialog = result.GetValue();
        
        // 转换为JSON
        json dialog_json = {
            {"id", updated_dialog.id},
            {"title", updated_dialog.title},
            {"model_id", updated_dialog.model_id},
            {"created_at", updated_dialog.created_at},
            {"updated_at", updated_dialog.updated_at},
            {"is_archived", updated_dialog.is_archived}
        };
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "更新成功"},
            {"data", dialog_json}
        });
        
    } catch (const json::exception& e) {
        spdlog::error("JSON error in UpdateDialog: {}", e.what());
        co_return Response::BadRequest({
            {"code", 400},
            {"message", "请求格式错误"},
            {"data", nullptr}
        });
    } catch (const std::exception& e) {
        spdlog::error("Error in UpdateDialog: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> DialogController::DeleteDialog(const Request& request) {
    try {
        // 获取用户ID
        if (!request.user_id.has_value()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", "未授权访问"},
                {"data", nullptr}
            });
        }
        
        // 获取对话ID
        std::string dialog_id = request.GetPathParam("id");
        
        // 验证用户是否有权访问此对话
        auto validation = co_await dialog_service_->ValidateDialogOwnership(dialog_id, request.user_id.value());
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 调用服务删除对话
        auto result = co_await dialog_service_->DeleteDialog(dialog_id);
        
        if (result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", result.GetError()},
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
        spdlog::error("Error in DeleteDialog: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

} // namespace ai_backend::api::controllers