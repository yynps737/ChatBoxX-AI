#include "services/ai/model_service.h"
#include <spdlog/spdlog.h>

namespace ai_backend::services::ai {

using namespace core::async;

ModelService& ModelService::GetInstance() {
    static ModelService instance;
    return instance;
}

void ModelService::Initialize() {
    // 初始化模型工厂
    ModelFactory::GetInstance().Initialize();
}

std::vector<std::string> ModelService::GetAvailableModels() const {
    return ModelFactory::GetInstance().GetAvailableModels();
}

common::Result<ModelService::ModelInfo> 
ModelService::GetModelInfo(const std::string& model_id) const {
    auto model = GetOrCreateModel(model_id);
    if (!model) {
        return common::Result<ModelInfo>::Error("Model not found: " + model_id);
    }
    
    ModelInfo info;
    info.id = model->GetModelId();
    info.name = model->GetModelName();
    info.provider = model->GetModelProvider();
    info.capabilities = model->GetCapabilities();
    info.supports_streaming = model->SupportsStreaming();
    
    return common::Result<ModelInfo>::Ok(info);
}

std::vector<ModelService::ModelInfo> ModelService::GetAllModelsInfo() const {
    std::vector<ModelInfo> result;
    auto available_models = GetAvailableModels();
    
    for (const auto& model_id : available_models) {
        auto info_result = GetModelInfo(model_id);
        if (info_result.IsOk()) {
            result.push_back(info_result.GetValue());
        }
    }
    
    return result;
}

Task<common::Result<std::string>> 
ModelService::GenerateResponse(const std::string& model_id,
                            const std::vector<models::Message>& messages,
                            const ModelInterface::ModelConfig& config) {
    auto model = GetOrCreateModel(model_id);
    if (!model) {
        co_return common::Result<std::string>::Error("Model not found: " + model_id);
    }
    
    if (!model->IsHealthy()) {
        co_return common::Result<std::string>::Error("Model is not healthy: " + model_id);
    }
    
    auto result = co_await model->GenerateResponse(messages, config);
    co_return result;
}

Task<common::Result<void>> 
ModelService::GenerateStreamingResponse(const std::string& model_id,
                                     const std::vector<models::Message>& messages,
                                     ModelInterface::StreamCallback callback,
                                     const ModelInterface::ModelConfig& config) {
    auto model = GetOrCreateModel(model_id);
    if (!model) {
        callback("", true); // 标记完成
        co_return common::Result<void>::Error("Model not found: " + model_id);
    }
    
    if (!model->IsHealthy()) {
        callback("", true); // 标记完成
        co_return common::Result<void>::Error("Model is not healthy: " + model_id);
    }
    
    if (!model->SupportsStreaming()) {
        // 回退到非流式API，然后模拟流式输出
        auto response_result = co_await model->GenerateResponse(messages, config);
        
        if (!response_result.IsOk()) {
            callback("", true); // 标记完成
            co_return common::Result<void>::Error(response_result.GetError());
        }
        
        const std::string& response = response_result.GetValue();
        
        // 模拟流式输出，每次发送一小部分文本
        const size_t chunk_size = 10;
        for (size_t i = 0; i < response.size(); i += chunk_size) {
            size_t length = std::min(chunk_size, response.size() - i);
            callback(response.substr(i, length), false);
            
            // 添加小延迟模拟真实流式输出
            co_await std::suspend_always{};
        }
        
        callback("", true); // 标记完成
        co_return common::Result<void>::Ok();
    }
    
    // 使用真正的流式API
    auto result = co_await model->GenerateStreamingResponse(messages, callback, config);
    co_return result;
}

common::Result<size_t> 
ModelService::GetLastPromptTokens(const std::string& model_id) const {
    auto model = GetOrCreateModel(model_id);
    if (!model) {
        return common::Result<size_t>::Error("Model not found: " + model_id);
    }
    
    return common::Result<size_t>::Ok(model->GetLastPromptTokens());
}

common::Result<size_t> 
ModelService::GetLastCompletionTokens(const std::string& model_id) const {
    auto model = GetOrCreateModel(model_id);
    if (!model) {
        return common::Result<size_t>::Error("Model not found: " + model_id);
    }
    
    return common::Result<size_t>::Ok(model->GetLastCompletionTokens());
}

common::Result<size_t> 
ModelService::GetLastTotalTokens(const std::string& model_id) const {
    auto model = GetOrCreateModel(model_id);
    if (!model) {
        return common::Result<size_t>::Error("Model not found: " + model_id);
    }
    
    return common::Result<size_t>::Ok(model->GetLastTotalTokens());
}

bool ModelService::IsModelHealthy(const std::string& model_id) const {
    auto model = GetOrCreateModel(model_id);
    if (!model) {
        return false;
    }
    
    return model->IsHealthy();
}

void ModelService::ResetModel(const std::string& model_id) {
    auto model = GetOrCreateModel(model_id);
    if (model) {
        model->Reset();
    }
}

std::shared_ptr<ModelInterface> 
ModelService::GetOrCreateModel(const std::string& model_id) const {
    // 先查找缓存
    {
        std::lock_guard<std::mutex> lock(models_mutex_);
        auto it = models_.find(model_id);
        if (it != models_.end()) {
            return it->second;
        }
    }
    
    // 创建新实例
    auto model = ModelFactory::GetInstance().CreateModel(model_id);
    if (model) {
        std::lock_guard<std::mutex> lock(models_mutex_);
        models_[model_id] = model;
    }
    
    return model;
}

} // namespace ai_backend::services::ai