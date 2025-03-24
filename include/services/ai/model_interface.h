#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/async/task.h"
#include "models/message.h"
#include "common/result.h"

namespace ai_backend::services::ai {

// 模型接口类，定义所有AI模型必须实现的接口
class ModelInterface {
public:
    // 模型参数配置
    struct ModelConfig {
        double temperature = 0.7;
        int max_tokens = 2048;
        double top_p = 0.9;
        double frequency_penalty = 0.0;
        double presence_penalty = 0.0;
        std::vector<std::string> stop_sequences;
        std::unordered_map<std::string, std::string> additional_params;
    };

    // 流式响应回调类型
    using StreamCallback = std::function<void(const std::string&, bool)>;

    virtual ~ModelInterface() = default;

    // 获取模型标识符
    virtual std::string GetModelId() const = 0;
    
    // 获取模型名称
    virtual std::string GetModelName() const = 0;
    
    // 获取模型提供商
    virtual std::string GetModelProvider() const = 0;
    
    // 获取模型能力描述
    virtual std::vector<std::string> GetCapabilities() const = 0;
    
    // 检查模型是否支持流式输出
    virtual bool SupportsStreaming() const = 0;

    // 非流式生成回复
    virtual core::async::Task<common::Result<std::string>> 
    GenerateResponse(const std::vector<models::Message>& messages, 
                    const ModelConfig& config = {}) = 0;
    
    // 流式生成回复
    virtual core::async::Task<common::Result<void>> 
    GenerateStreamingResponse(const std::vector<models::Message>& messages,
                             StreamCallback callback,
                             const ModelConfig& config = {}) = 0;
    
    // 获取最后请求的Token数量
    virtual size_t GetLastPromptTokens() const = 0;
    
    // 获取最后响应的Token数量
    virtual size_t GetLastCompletionTokens() const = 0;
    
    // 获取最后一次请求的总Token数量
    virtual size_t GetLastTotalTokens() const = 0;
    
    // 获取模型服务状态
    virtual bool IsHealthy() const = 0;
    
    // 重置模型状态
    virtual void Reset() = 0;
};

} // namespace ai_backend::services::ai