#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "model_interface.h"

namespace ai_backend::services::ai {

// 模型工厂类，用于创建和管理AI模型实例
class ModelFactory {
public:
    // 获取单例实例
    static ModelFactory& GetInstance();

    // 禁止拷贝和移动
    ModelFactory(const ModelFactory&) = delete;
    ModelFactory& operator=(const ModelFactory&) = delete;
    ModelFactory(ModelFactory&&) = delete;
    ModelFactory& operator=(ModelFactory&&) = delete;

    // 注册模型创建器
    template <typename ModelType>
    void RegisterModel() {
        auto model = std::make_shared<ModelType>();
        std::string modelId = model->GetModelId();
        model_creators_[modelId] = [](){ return std::make_shared<ModelType>(); };
        available_models_.push_back(modelId);
    }

    // 创建模型实例
    std::shared_ptr<ModelInterface> CreateModel(const std::string& model_id);
    
    // 获取可用模型列表
    const std::vector<std::string>& GetAvailableModels() const;
    
    // 初始化所有支持的模型
    void Initialize();

private:
    ModelFactory() = default;

    using ModelCreator = std::function<std::shared_ptr<ModelInterface>()>;
    std::unordered_map<std::string, ModelCreator> model_creators_;
    std::vector<std::string> available_models_;
};

} // namespace ai_backend::services::ai