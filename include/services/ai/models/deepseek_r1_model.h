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

// DeepSeek R1 模型实现
class DeepseekR1Model : public ModelInterface {
public:
    DeepseekR1Model();
    ~DeepseekR1Model() override;

    // ModelInterface接口实现
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
    // 构建API请求
    core::http::Request BuildAPIRequest(const std::vector<models::Message>& messages,
                                      const ModelConfig& config,
                                      bool stream) const;
    
    // 解析API响应
    common::Result<std::string> ParseAPIResponse(const std::string& response);
    
    // 处理流式响应
    void HandleStreamChunk(const std::string& chunk, StreamCallback callback, bool& is_done);
    
    // 计算并记录token数量
    void CalculateAndStoreTokenCounts(const std::vector<models::Message>& messages, 
                                     const std::string& response);

private:
    std::string api_key_;
    std::string api_base_url_;
    std::atomic<size_t> last_prompt_tokens_;
    std::atomic<size_t> last_completion_tokens_;
    std::atomic<bool> is_healthy_;
    
    // 支持的模型列表
    static constexpr const char* MODEL_ID = "deepseek-r1";
    static constexpr const char* MODEL_NAME = "DeepSeek Coder R1";
    static constexpr const char* MODEL_PROVIDER = "DeepSeek";
};

} // namespace ai_backend::services::ai