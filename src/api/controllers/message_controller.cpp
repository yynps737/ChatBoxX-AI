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

Task<std::vector<models::Message>> MessageController::BuildMessageContext(
    const std::string& dialog_id,
    const std::string& message_id) {
    
    auto messages_result = co_await message_service_->GetAllMessagesByDialogId(dialog_id);
    if (messages_result.IsError()) {
        throw std::runtime_error(messages_result.GetError());
    }
    
    auto messages = messages_result.GetValue();
    
    models::Message user_message;
    bool found = false;
    
    for (const auto& msg : messages) {
        if (msg.id == message_id) {
            user_message = msg;
            found = true;
            break;
        }
    }
    
    if (!found) {
        throw std::runtime_error("消息不存在");
    }
    
    std::vector<models::Message> context;
    
    for (const auto& msg : messages) {
        if (msg.role == "system") {
            context.push_back(msg);
            break;
        }
    }
    
    std::vector<models::Message> history;
    for (const auto& msg : messages) {
        if (msg.role != "system" && msg.id != message_id) {
            history.push_back(msg);
        }
    }
    
    std::sort(history.begin(), history.end(), [](const models::Message& a, const models::Message& b) {
        return a.created_at < b.created_at;
    });
    
    if (history.size() > 10) {
        history.erase(history.begin(), history.end() - 10);
    }
    
    context.insert(context.end(), history.begin(), history.end());
    context.push_back(user_message);
    
    co_return context;
}

services::ai::ModelInterface::ModelConfig MessageController::BuildModelConfig() {
    services::ai::ModelInterface::ModelConfig config;
    config.temperature = 0.7;
    config.max_tokens = 2048;
    return config;
}

Task<Response> MessageController::GetReply(const Request& request) {
    try {
        bool stream = request.GetQueryParam("stream", "false") == "true";
        if (stream) {
            co_return co_await GetStreamReply(request);
        }
        
        std::string dialog_id = request.GetPathParam("dialog_id");
        std::string message_id = request.GetPathParam("message_id");
        
        auto validation = ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            co_return Response::Forbidden({
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            });
        }
        
        auto dialog_result = co_await dialog_service_->GetDialogById(dialog_id);
        if (dialog_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", dialog_result.GetError()},
                {"data", nullptr}
            });
        }
        
        auto context = co_await BuildMessageContext(dialog_id, message_id);
        auto model_config = BuildModelConfig();
        
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
        
        models::Message ai_message;
        ai_message.dialog_id = dialog_id;
        ai_message.role = "assistant";
        ai_message.content = response_result.GetValue();
        ai_message.type = "text";
        
        auto save_result = co_await message_service_->CreateMessage(ai_message);
        
        if (save_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", save_result.GetError()},
                {"data", nullptr}
            });
        }
        
        const auto& created_ai_message = save_result.GetValue();
        
        json message_json = {
            {"id", created_ai_message.id},
            {"dialog_id", created_ai_message.dialog_id},
            {"role", created_ai_message.role},
            {"content", created_ai_message.content},
            {"type", created_ai_message.type},
            {"created_at", created_ai_message.created_at},
            {"tokens", created_ai_message.tokens}
        };
        
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

void MessageController::HandleStreamingResponse(
    const std::string& content,
    bool is_done,
    StreamWriter& writer) {
    
    if (is_done) {
        writer.Write("data: [DONE]\n\n");
        writer.End();
        return;
    }
    
    std::string escaped_content = content;
    json::escape_string(escaped_content);
    
    std::string data = "data: {\"delta\":\"" + escaped_content + "\"}\n\n";
    writer.Write(data);
}
}