#pragma once

#include <memory>
#include <string>

#include "core/http/request.h"
#include "core/http/response.h"
#include "core/async/task.h"
#include "services/message/message_service.h"
#include "services/dialog/dialog_service.h"
#include "services/ai/model_service.h"

namespace ai_backend::api::controllers {

// 消息控制器，处理所有消息相关的API请求
class MessageController {
public:
    MessageController(std::shared_ptr<services::message::MessageService> message_service,
                     std::shared_ptr<services::dialog::DialogService> dialog_service);
    
    // 获取对话的消息列表
    core::async::Task<core::http::Response> GetMessages(const core::http::Request& request);
    
    // 创建新消息
    core::async::Task<core::http::Response> CreateMessage(const core::http::Request& request);
    
    // 获取AI回复
    core::async::Task<core::http::Response> GetReply(const core::http::Request& request);
    
    // 获取流式AI回复
    core::async::Task<core::http::Response> GetStreamReply(const core::http::Request& request);
    
    // 删除消息
    core::async::Task<core::http::Response> DeleteMessage(const core::http::Request& request);

private:
    // 验证对话访问权限
    common::Result<std::string> ValidateDialogAccess(
        const core::http::Request& request, 
        const std::string& dialog_id
    );
    
    // 处理流式回复
    void HandleStreamingResponse(
        const std::string& content,
        bool is_done,
        core::http::StreamWriter& writer
    );

private:
    std::shared_ptr<services::message::MessageService> message_service_;
    std::shared_ptr<services::dialog::DialogService> dialog_service_;
    services::ai::ModelService& model_service_;
};

} // namespace ai_backend::api::controllers