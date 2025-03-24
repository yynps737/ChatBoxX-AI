#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>

#include "services/ai/model_interface.h"
#include "core/http/request.h"
#include "core/async/task.h"
#include "common/result.h"

namespace ai_backend::services::ai {

class XunfeiModel : public ModelInterface {
public:
    XunfeiModel();
    ~XunfeiModel() override;

    std::string GetModelId() const override;
    std::string GetModelName() const override;
    std::string GetModelProvider() const override;
    std::vector<std::string> GetCapabilities() const override;
    bool SupportsStreaming() const override;

    core::async::Task<common::Result<std::string>> 
    GenerateResponse(const std::vector<models::Message>& messages, 
                    const ModelConfig& config = {}) override;
    
    core::async::Task<common::Result<void>> 
    GenerateStreamingResponse(const std::vector<models::Message>& messages,
                             StreamCallback callback,
                             const ModelConfig& config = {}) override;
    
    size_t GetLastPromptTokens() const override;
    size_t GetLastCompletionTokens() const override;
    size_t GetLastTotalTokens() const override;
    bool IsHealthy() const override;
    void Reset() override;

private:
    // 生成带鉴权的URL
    std::string GenerateAuthUrl() const;
    
    // 构建请求体
    nlohmann::json BuildRequestBody(const std::vector<models::Message>& messages, 
                                  const ModelConfig& config) const;

    // 简单的WebSocket客户端
    class WebSocketClient {
    public:
        WebSocketClient();
        ~WebSocketClient();
        
        core::async::Task<bool> Connect(const std::string& url);
        core::async::Task<bool> Send(const std::string& message);
        core::async::Task<std::string> Receive();
        void Close();
        
    private:
        bool is_connected_;
    };

private:
    std::string app_id_;
    std::string api_key_;
    std::string api_secret_;
    std::string api_base_url_;
    std::atomic<size_t> last_prompt_tokens_;
    std::atomic<size_t> last_completion_tokens_;
    std::atomic<bool> is_healthy_;
    
    static constexpr const char* MODEL_ID = "xunfei-spark";
    static constexpr const char* MODEL_NAME = "讯飞星火认知大模型";
    static constexpr const char* MODEL_PROVIDER = "科大讯飞";
};

} // namespace ai_backend::services::ai