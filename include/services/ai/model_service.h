#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "services/ai/model_interface.h"
#include "services/ai/model_factory.h"
#include "models/message.h"
#include "core/async/task.h"
#include "common/result.h"

namespace ai_backend::services::ai {

// 模型服务类，为控制器提供统一的AI模型访问接口
class ModelService {
public:
    // 单例访问
    static ModelService& GetInstance();

    // 初始化服务
    void Initialize();

    // 获取支持的所有模型列表
    std::vector<std::string> GetAvailableModels() const;

    // 获取模型详情
    struct ModelInfo {
        std::string id;
        std::string name;
        std::string provider;
        std::vector<std::string> capabilities;
        bool supports_streaming;
    };
    
    common::Result<ModelInfo> GetModelInfo(const std::string& model_id) const;
    std::vector<ModelInfo> GetAllModelsInfo() const;

    // 使用指定模型生成回复
    core::async::Task<common::Result<std::string>> 
    GenerateResponse(const std::string& model_id,
                    const std::vector<models::Message>& messages,
                    const ModelInterface::ModelConfig& config = {});

    // 使用指定模型生成流式回复
    core::async::Task<common::Result<void>> 
    GenerateStreamingResponse(const std::string& model_id,
                             const std::vector<models::Message>& messages,
                             ModelInterface::StreamCallback callback,
                             const ModelInterface::ModelConfig& config = {});

    // 获取指定模型的Token使用情况
    common::Result<size_t> GetLastPromptTokens(const std::string& model_id) const;
    common::Result<size_t> GetLastCompletionTokens(const std::string& model_id) const;
    common::Result<size_t> GetLastTotalTokens(const std::string& model_id) const;

    // 检查模型健康状态
    bool IsModelHealthy(const std::string& model_id) const;
    
    // 重置指定模型状态
    void ResetModel(const std::string& model_id);

private:
    ModelService() = default;
    
    // 禁止拷贝和移动
    ModelService(const ModelService&) = delete;
    ModelService& operator=(const ModelService&) = delete;
    ModelService(ModelService&&) = delete;
    ModelService& operator=(ModelService&&) = delete;

    // 获取模型实例，如果不存在则创建
    std::shared_ptr<ModelInterface> GetOrCreateModel(const std::string& model_id);

private:
    // 缓存模型实例
    std::unordered_map<std::string, std::shared_ptr<ModelInterface>> models_;
    mutable std::mutex models_mutex_;
};

} // namespace ai_backend::services::ai