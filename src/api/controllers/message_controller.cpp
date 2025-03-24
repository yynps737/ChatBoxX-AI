#include "api/controllers/message_controller.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai_backend::api::controllers {

using json = nlohmann::json;
using namespace core::async;
using namespace core::http;

MessageController::MessageController(
    std::shared_ptr<services::message::MessageService> message_service,
    std::shared_ptr<services::dialog::DialogService> dialog_service)
    : message_service_(std::move(message_service)),
      dialog_service_(std::move(dialog_service)),
      model_service_(services::ai::ModelService::GetInstance()) {
}

Task<Response> MessageController::GetMessages(const Request& request) {
    try {
        // 从URL获取对话ID
        std::string dialog_id = request.GetPathParam("dialog_id");
        
        // 验证用户是否有权访问此对话
        auto validation = ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取分页参数
        int page = std::stoi(request.GetQueryParam("page", "1"));
        int page_size = std::stoi(request.GetQueryParam("page_size", "20"));
        
        // 获取消息列表
        auto result = co_await message_service_->GetMessagesByDialogId(dialog_id, page, page_size);
        
        if (result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 转换结果为JSON
        json messages_json = json::array();
        for (const auto& message : result.GetValue()) {
            json message_json = {
                {"id", message.id},
                {"dialog_id", message.dialog_id},
                {"role", message.role},
                {"content", message.content},
                {"type", message.type},
                {"created_at", message.created_at},
                {"tokens", message.tokens}
            };
            
            if (!message.attachments.empty()) {
                json attachments_json = json::array();
                for (const auto& attachment : message.attachments) {
                    attachments_json.push_back({
                        {"id", attachment.id},
                        {"name", attachment.name},
                        {"type", attachment.type},
                        {"url", attachment.url}
                    });
                }
                message_json["attachments"] = attachments_json;
            }
            
            messages_json.push_back(message_json);
        }
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "获取成功"},
            {"data", {
                {"messages", messages_json},
                {"total", result.GetValue().size()},
                {"page", page},
                {"page_size", page_size}
            }}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetMessages: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> MessageController::CreateMessage(const Request& request) {
    try {
        // 从URL获取对话ID
        std::string dialog_id = request.GetPathParam("dialog_id");
        
        // 验证用户是否有权访问此对话
        auto validation = ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 解析请求体JSON
        json request_body = json::parse(request.body);
        
        // 验证请求体
        if (!request_body.contains("content") || !request_body["content"].is_string()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", "消息内容不能为空"},
                {"data", nullptr}
            });
        }
        
        // 创建消息对象
        models::Message message;
        message.dialog_id = dialog_id;
        message.role = "user";
        message.content = request_body["content"].get<std::string>();
        message.type = request_body.value("type", "text");
        
        // 处理附件
        if (request_body.contains("attachments") && request_body["attachments"].is_array()) {
            for (const auto& attachment : request_body["attachments"]) {
                if (attachment.contains("id") && attachment.contains("type")) {
                    models::Attachment att;
                    att.id = attachment["id"].get<std::string>();
                    att.type = attachment["type"].get<std::string>();
                    if (attachment.contains("name")) {
                        att.name = attachment["name"].get<std::string>();
                    }
                    message.attachments.push_back(att);
                }
            }
        }
        
        // 创建消息
        auto result = co_await message_service_->CreateMessage(message);
        
        if (result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取创建的消息
        const auto& created_message = result.GetValue();
        
        // 转换为JSON
        json message_json = {
            {"id", created_message.id},
            {"dialog_id", created_message.dialog_id},
            {"role", created_message.role},
            {"content", created_message.content},
            {"type", created_message.type},
            {"created_at", created_message.created_at},
            {"tokens", created_message.tokens}
        };
        
        if (!created_message.attachments.empty()) {
            json attachments_json = json::array();
            for (const auto& attachment : created_message.attachments) {
                attachments_json.push_back({
                    {"id", attachment.id},
                    {"name", attachment.name},
                    {"type", attachment.type},
                    {"url", attachment.url}
                });
            }
            message_json["attachments"] = attachments_json;
        }
        
        // 返回成功响应
        co_return Response::Created({
            {"code", 0},
            {"message", "发送成功"},
            {"data", message_json}
        });
        
    } catch (const json::exception& e) {
        spdlog::error("JSON error in CreateMessage: {}", e.what());
        co_return Response::BadRequest({
            {"code", 400},
            {"message", "请求格式错误"},
            {"data", nullptr}
        });
    } catch (const std::exception& e) {
        spdlog::error("Error in CreateMessage: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> MessageController::GetReply(const Request& request) {
    try {
        // 检查是否请求流式响应
        bool stream = request.GetQueryParam("stream", "false") == "true";
        if (stream) {
            co_return co_await GetStreamReply(request);
        }
        
        // 从URL获取对话ID和消息ID
        std::string dialog_id = request.GetPathParam("dialog_id");
        std::string message_id = request.GetPathParam("message_id");
        
        // 验证用户是否有权访问此对话
        auto validation = ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取对话信息以获取模型ID
        auto dialog_result = co_await dialog_service_->GetDialogById(dialog_id);
        if (dialog_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", dialog_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取对话中的所有消息，以构建对话上下文
        auto messages_result = co_await message_service_->GetAllMessagesByDialogId(dialog_id);
        if (messages_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", messages_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取指定用户消息，确保其存在
        models::Message user_message;
        bool found = false;
        
        for (const auto& msg : messages_result.GetValue()) {
            if (msg.id == message_id) {
                user_message = msg;
                found = true;
                break;
            }
        }
        
        if (!found) {
            co_return Response::NotFound({
                {"code", 404},
                {"message", "消息不存在"},
                {"data", nullptr}
            });
        }
        
        // 获取系统提示消息和对话历史
        std::vector<models::Message> context;
        
        // 首先添加系统提示消息（如果有）
        for (const auto& msg : messages_result.GetValue()) {
            if (msg.role == "system") {
                context.push_back(msg);
                break;
            }
        }
        
        // 然后添加最近的消息历史（最多10条）
        std::vector<models::Message> history;
        for (const auto& msg : messages_result.GetValue()) {
            if (msg.role != "system" && msg.id != message_id) {
                history.push_back(msg);
            }
        }
        
        // 按时间排序，并保留最近的消息
        std::sort(history.begin(), history.end(), [](const models::Message& a, const models::Message& b) {
            return a.created_at < b.created_at;
        });
        
        // 如果历史消息超过10条，只保留最近的10条
        if (history.size() > 10) {
            history.erase(history.begin(), history.end() - 10);
        }
        
        // 添加历史消息和当前用户消息到上下文
        context.insert(context.end(), history.begin(), history.end());
        context.push_back(user_message);
        
        // 配置模型参数
        services::ai::ModelInterface::ModelConfig model_config;
        model_config.temperature = 0.7;
        model_config.max_tokens = 2048;
        
        // 调用模型生成回复
        auto response_result = co_await model_service_.GenerateResponse(
            dialog_result.GetValue().model_id,
            context,
            model_config
        );
        
        if (response_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", response_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 创建AI回复消息
        models::Message ai_message;
        ai_message.dialog_id = dialog_id;
        ai_message.role = "assistant";
        ai_message.content = response_result.GetValue();
        ai_message.type = "text";
        
        // 保存AI回复到数据库
        auto save_result = co_await message_service_->CreateMessage(ai_message);
        
        if (save_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", save_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取创建的AI消息
        const auto& created_ai_message = save_result.GetValue();
        
        // 转换为JSON
        json message_json = {
            {"id", created_ai_message.id},
            {"dialog_id", created_ai_message.dialog_id},
            {"role", created_ai_message.role},
            {"content", created_ai_message.content},
            {"type", created_ai_message.type},
            {"created_at", created_ai_message.created_at},
            {"tokens", created_ai_message.tokens}
        };
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "回复成功"},
            {"data", message_json}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetReply: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> MessageController::GetStreamReply(const Request& request) {
    try {
        // 从URL获取对话ID和消息ID
        std::string dialog_id = request.GetPathParam("dialog_id");
        std::string message_id = request.GetPathParam("message_id");
        
        // 验证用户是否有权访问此对话
        auto validation = ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取对话信息以获取模型ID
        auto dialog_result = co_await dialog_service_->GetDialogById(dialog_id);
        if (dialog_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", dialog_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取对话中的所有消息，以构建对话上下文
        auto messages_result = co_await message_service_->GetAllMessagesByDialogId(dialog_id);
        if (messages_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", messages_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 获取指定用户消息，确保其存在
        models::Message user_message;
        bool found = false;
        
        for (const auto& msg : messages_result.GetValue()) {
            if (msg.id == message_id) {
                user_message = msg;
                found = true;
                break;
            }
        }
        
        if (!found) {
            co_return Response::NotFound({
                {"code", 404},
                {"message", "消息不存在"},
                {"data", nullptr}
            });
        }
        
        // 获取系统提示消息和对话历史
        std::vector<models::Message> context;
        
        // 首先添加系统提示消息（如果有）
        for (const auto& msg : messages_result.GetValue()) {
            if (msg.role == "system") {
                context.push_back(msg);
                break;
            }
        }
        
        // 然后添加最近的消息历史（最多10条）
        std::vector<models::Message> history;
        for (const auto& msg : messages_result.GetValue()) {
            if (msg.role != "system" && msg.id != message_id) {
                history.push_back(msg);
            }
        }
        
        // 按时间排序，并保留最近的消息
        std::sort(history.begin(), history.end(), [](const models::Message& a, const models::Message& b) {
            return a.created_at < b.created_at;
        });
        
        // 如果历史消息超过10条，只保留最近的10条
        if (history.size() > 10) {
            history.erase(history.begin(), history.end() - 10);
        }
        
        // 添加历史消息和当前用户消息到上下文
        context.insert(context.end(), history.begin(), history.end());
        context.push_back(user_message);
        
        // 配置模型参数
        services::ai::ModelInterface::ModelConfig model_config;
        model_config.temperature = 0.7;
        model_config.max_tokens = 2048;
        
        // 创建流式响应
        Response response;
        response.status_code = 200;
        response.headers["Content-Type"] = "text/event-stream";
        response.headers["Cache-Control"] = "no-cache";
        response.headers["Connection"] = "keep-alive";
        response.headers["Transfer-Encoding"] = "chunked";
        
        // 设置流式写入器
        std::shared_ptr<std::string> full_content = std::make_shared<std::string>();
        
        response.stream_handler = [this, dialog_id, full_content](StreamWriter& writer) -> Task<void> {
            // 先发送一个初始化消息
            writer.Write("data: {\"delta\":\"\"}\n\n");
            
            // 创建回调函数，用于处理流式输出
            services::ai::ModelInterface::StreamCallback callback = 
                [this, &writer, full_content](const std::string& content, bool is_done) {
                    *full_content += content;
                    this->HandleStreamingResponse(content, is_done, writer);
                };
            
            // 调用模型生成流式回复
            auto result = co_await model_service_.GenerateStreamingResponse(
                dialog_result.GetValue().model_id,
                context,
                callback,
                model_config
            );
            
            if (result.IsError()) {
                spdlog::error("Error in streaming response: {}", result.GetError());
                writer.Write("data: {\"error\":\"" + result.GetError() + "\"}\n\n");
                writer.Write("data: [DONE]\n\n");
                writer.End();
                co_return;
            }
            
            // 创建AI回复消息并保存到数据库
            models::Message ai_message;
            ai_message.dialog_id = dialog_id;
            ai_message.role = "assistant";
            ai_message.content = *full_content;
            ai_message.type = "text";
            
            // 异步保存消息，但不等待结果
            message_service_->CreateMessage(ai_message).detach();
            
            co_return;
        };
        
        co_return response;
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetStreamReply: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> MessageController::DeleteMessage(const Request& request) {
    try {
        // 从URL获取对话ID和消息ID
        std::string dialog_id = request.GetPathParam("dialog_id");
        std::string message_id = request.GetPathParam("message_id");
        
        // 验证用户是否有权访问此对话
        auto validation = ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        // 删除消息
        auto result = co_await message_service_->DeleteMessage(message_id);
        
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
        spdlog::error("Error in DeleteMessage: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

common::Result<std::string> MessageController::ValidateDialogAccess(
    const Request& request, 
    const std::string& dialog_id
) {
    // 从请求中获取用户ID
    if (!request.user_id.has_value()) {
        return common::Result<std::string>::Error("未授权访问");
    }
    
    std::string user_id = request.user_id.value();
    
    // 调用对话服务检查访问权限
    auto result = dialog_service_->ValidateDialogOwnership(dialog_id, user_id);
    
    return result;
}

void MessageController::HandleStreamingResponse(
    const std::string& content,
    bool is_done,
    StreamWriter& writer
) {
    if (is_done) {
        writer.Write("data: [DONE]\n\n");
        writer.End();
        return;
    }
    
    // 转义JSON特殊字符
    std::string escaped_content = content;
    json::escape_string(escaped_content);
    
    // 发送增量更新
    std::string data = "data: {\"delta\":\"" + escaped_content + "\"}\n\n";
    writer.Write(data);
}

} // namespace ai_backend::api::controllers