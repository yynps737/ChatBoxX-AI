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
        std::string dialog_id = request.GetPathParam("dialog_id");
        
        auto validation = co_await ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            json error_json = {
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            };
            co_return Response::Forbidden(error_json);
        }
        
        int page = std::stoi(request.GetQueryParam("page", "1"));
        int page_size = std::stoi(request.GetQueryParam("page_size", "50"));
        
        auto result = co_await message_service_->GetMessagesByDialogId(dialog_id, page, page_size);
        
        if (result.IsError()) {
            json error_json = {
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            };
            co_return Response::InternalServerError(error_json);
        }
        
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
                        {"type", attachment.type},
                        {"name", attachment.name},
                        {"url", attachment.url}
                    });
                }
                message_json["attachments"] = attachments_json;
            }
            
            messages_json.push_back(message_json);
        }
        
        json response_json = {
            {"code", 0},
            {"message", "获取成功"},
            {"data", {
                {"messages", messages_json},
                {"total", messages_json.size()},
                {"page", page},
                {"page_size", page_size}
            }}
        };
        
        co_return Response::OK(response_json);
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetMessages: {}", e.what());
        json error_json = {
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        };
        co_return Response::InternalServerError(error_json);
    }
}

Task<Response> MessageController::CreateMessage(const Request& request) {
    try {
        std::string dialog_id = request.GetPathParam("dialog_id");
        
        auto validation = co_await ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            json error_json = {
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            };
            co_return Response::Forbidden(error_json);
        }
        
        json request_body = json::parse(request.body);
        
        if (!request_body.contains("content") || !request_body["content"].is_string()) {
            json error_json = {
                {"code", 400},
                {"message", "消息内容不能为空"},
                {"data", nullptr}
            };
            co_return Response::BadRequest(error_json);
        }
        
        models::Message message;
        message.dialog_id = dialog_id;
        message.role = request_body.value("role", "user");
        message.content = request_body["content"].get<std::string>();
        message.type = request_body.value("type", "text");
        
        auto token_result = co_await message_service_->CountTokens(message.content);
        if (token_result.IsOk()) {
            message.tokens = token_result.GetValue();
        }
        
        if (request_body.contains("attachments") && request_body["attachments"].is_array()) {
            for (const auto& attachment_json : request_body["attachments"]) {
                models::Attachment attachment;
                attachment.id = attachment_json["id"].get<std::string>();
                attachment.type = attachment_json["type"].get<std::string>();
                
                if (attachment_json.contains("name")) {
                    attachment.name = attachment_json["name"].get<std::string>();
                }
                
                if (attachment_json.contains("url")) {
                    attachment.url = attachment_json["url"].get<std::string>();
                }
                
                message.attachments.push_back(attachment);
            }
        }
        
        auto result = co_await message_service_->CreateMessage(message);
        
        if (result.IsError()) {
            json error_json = {
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            };
            co_return Response::InternalServerError(error_json);
        }
        
        const auto& created_message = result.GetValue();
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
                    {"type", attachment.type},
                    {"name", attachment.name},
                    {"url", attachment.url}
                });
            }
            message_json["attachments"] = attachments_json;
        }
        
        co_return Response::Created(message_json);
        
    } catch (const json::exception& e) {
        spdlog::error("JSON error in CreateMessage: {}", e.what());
        json error_json = {
            {"code", 400},
            {"message", "请求格式错误"},
            {"data", nullptr}
        };
        co_return Response::BadRequest(error_json);
    } catch (const std::exception& e) {
        spdlog::error("Error in CreateMessage: {}", e.what());
        json error_json = {
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        };
        co_return Response::InternalServerError(error_json);
    }
}

// 添加缺失的方法声明
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
        
        auto validation = co_await ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            json error_json = {
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            };
            co_return Response::Forbidden(error_json);
        }
        
        auto dialog_result = co_await dialog_service_->GetDialogById(dialog_id);
        if (dialog_result.IsError()) {
            json error_json = {
                {"code", 500},
                {"message", dialog_result.GetError()},
                {"data", nullptr}
            };
            co_return Response::InternalServerError(error_json);
        }
        
        auto context = co_await BuildMessageContext(dialog_id, message_id);
        auto model_config = BuildModelConfig();
        
        auto response_result = co_await model_service_.GenerateResponse(
            dialog_result.GetValue().model_id,
            context,
            model_config
        );
        
        if (response_result.IsError()) {
            json error_json = {
                {"code", 500},
                {"message", response_result.GetError()},
                {"data", nullptr}
            };
            co_return Response::InternalServerError(error_json);
        }
        
        models::Message ai_message;
        ai_message.dialog_id = dialog_id;
        ai_message.role = "assistant";
        ai_message.content = response_result.GetValue();
        ai_message.type = "text";
        
        auto save_result = co_await message_service_->CreateMessage(ai_message);
        
        if (save_result.IsError()) {
            json error_json = {
                {"code", 500},
                {"message", save_result.GetError()},
                {"data", nullptr}
            };
            co_return Response::InternalServerError(error_json);
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
        
        json response_json = {
            {"code", 0},
            {"message", "回复成功"},
            {"data", message_json}
        };
        
        co_return Response::OK(response_json);
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetReply: {}", e.what());
        json error_json = {
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        };
        co_return Response::InternalServerError(error_json);
    }
}

Task<Response> MessageController::GetStreamReply(const Request& request) {
    try {
        std::string dialog_id = request.GetPathParam("dialog_id");
        std::string message_id = request.GetPathParam("message_id");
        
        auto validation = co_await ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            json error_json = {
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            };
            co_return Response::Forbidden(error_json);
        }
        
        auto dialog_result = co_await dialog_service_->GetDialogById(dialog_id);
        if (dialog_result.IsError()) {
            json error_json = {
                {"code", 500},
                {"message", dialog_result.GetError()},
                {"data", nullptr}
            };
            co_return Response::InternalServerError(error_json);
        }
        
        auto context = co_await BuildMessageContext(dialog_id, message_id);
        auto model_config = BuildModelConfig();
        
        Response response;
        response.status_code = 200;
        response.headers["Content-Type"] = "text/event-stream";
        response.headers["Cache-Control"] = "no-cache";
        response.headers["Connection"] = "keep-alive";
        response.headers["X-Accel-Buffering"] = "no";
        
        std::string generated_content;
        std::string generated_message_id;
        
        response.stream_handler = [this, 
                                  &generated_content, 
                                  &generated_message_id,
                                  dialog_id,
                                  context,
                                  model_config,
                                  model_id = dialog_result.GetValue().model_id](StreamWriter& writer) -> Task<void> {
            try {
                co_await model_service_.GenerateStreamingResponse(
                    model_id,
                    context,
                    [&](const std::string& content, bool is_done) {
                        if (is_done) {
                            if (!generated_content.empty()) {
                                models::Message ai_message;
                                ai_message.dialog_id = dialog_id;
                                ai_message.role = "assistant";
                                ai_message.content = generated_content;
                                ai_message.type = "text";
                                
                                auto save_result = message_service_->CreateMessage(ai_message);
                                
                                if (co_await save_result.await_ready()) {
                                    auto result = co_await save_result;
                                    if (result.IsOk()) {
                                        generated_message_id = result.GetValue().id;
                                    }
                                }
                            }
                            
                            std::string final_data;
                            if (!generated_message_id.empty()) {
                                json final_json = {
                                    {"id", generated_message_id},
                                    {"dialog_id", dialog_id},
                                    {"role", "assistant"},
                                    {"content", generated_content},
                                    {"type", "text"}
                                };
                                final_data = "data: " + final_json.dump() + "\n\n";
                            }
                            
                            final_data += "data: [DONE]\n\n";
                            writer.Write(final_data);
                            writer.End();
                        } else {
                            generated_content += content;
                            HandleStreamingResponse(content, is_done, writer);
                        }
                    },
                    model_config
                );
            } catch (const std::exception& e) {
                spdlog::error("Error in stream processing: {}", e.what());
                writer.Write("data: {\"error\":\"" + std::string(e.what()) + "\"}\n\n");
                writer.Write("data: [DONE]\n\n");
                writer.End();
            }
            
            co_return;
        };
        
        co_return response;
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetStreamReply: {}", e.what());
        json error_json = {
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        };
        co_return Response::InternalServerError(error_json);
    }
}

Task<Response> MessageController::DeleteMessage(const Request& request) {
    try {
        std::string dialog_id = request.GetPathParam("dialog_id");
        std::string message_id = request.GetPathParam("message_id");
        
        auto validation = co_await ValidateDialogAccess(request, dialog_id);
        if (validation.IsError()) {
            json error_json = {
                {"code", 403},
                {"message", validation.GetError()},
                {"data", nullptr}
            };
            co_return Response::Forbidden(error_json);
        }
        
        auto result = co_await message_service_->DeleteMessage(message_id);
        
        if (result.IsError()) {
            json error_json = {
                {"code", 500},
                {"message", result.GetError()},
                {"data", nullptr}
            };
            co_return Response::InternalServerError(error_json);
        }
        
        json response_json = {
            {"code", 0},
            {"message", "删除成功"},
            {"data", nullptr}
        };
        
        co_return Response::OK(response_json);
        
    } catch (const std::exception& e) {
        spdlog::error("Error in DeleteMessage: {}", e.what());
        json error_json = {
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        };
        co_return Response::InternalServerError(error_json);
    }
}

common::Result<std::string> MessageController::ValidateDialogAccess(
    const Request& request, 
    const std::string& dialog_id) {
    
    if (!request.user_id.has_value()) {
        return common::Result<std::string>::Error("未授权访问");
    }
    
    auto result = co_await dialog_service_->ValidateDialogOwnership(dialog_id, request.user_id.value());
    
    co_return result;
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
    // 使用适当的转义方法，不使用json::escape_string
    std::string escaped = "";
    for (char c : content) {
        if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f')) {
            escaped += '\\';
            switch(c) {
                case '"': escaped += '"'; break;
                case '\\': escaped += '\\'; break;
                case '\b': escaped += 'b'; break;
                case '\f': escaped += 'f'; break;
                case '\n': escaped += 'n'; break;
                case '\r': escaped += 'r'; break;
                case '\t': escaped += 't'; break;
                default:
                    char buf[8];
                    snprintf(buf, sizeof buf, "u%04x", c);
                    escaped += buf;
            }
        } else {
            escaped += c;
        }
    }
    
    std::string data = "data: {\"delta\":\"" + escaped + "\"}\n\n";
    writer.Write(data);
}

} // namespace ai_backend::api::controllers
