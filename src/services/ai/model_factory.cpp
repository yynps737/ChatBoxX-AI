#include "services/ai/model_factory.h"
#include "services/ai/models/deepseek_r1_model.h"
#include "services/ai/models/deepseek_v3_model.h"
#include <spdlog/spdlog.h>

namespace ai_backend::services::ai {

ModelFactory& ModelFactory::GetInstance() {
    static ModelFactory instance;
    return instance;
}

std::shared_ptr<ModelInterface> ModelFactory::CreateModel(const std::string& model_id) {
    auto it = model_creators_.find(model_id);
    if (it == model_creators_.end()) {
        spdlog::warn("Attempted to create unknown model: {}", model_id);
        return nullptr;
    }
    
    spdlog::debug("Creating model instance: {}", model_id);
    return it->second();
}

const std::vector<std::string>& ModelFactory::GetAvailableModels() const {
    return available_models_;
}

void ModelFactory::Initialize() {
    // Register only DeepSeek models
    spdlog::info("Initializing ModelFactory with DeepSeek models");
    RegisterModel<DeepseekR1Model>();
    RegisterModel<DeepseekV3Model>();
    
    spdlog::info("ModelFactory initialized with {} models", available_models_.size());
    for (const auto& model : available_models_) {
        spdlog::info("Available model: {}", model);
    }
}

template <typename ModelType>
void ModelFactory::RegisterModel() {
    auto model = std::make_shared<ModelType>();
    std::string model_id = model->GetModelId();
    
    model_creators_[model_id] = [](){ return std::make_shared<ModelType>(); };
    available_models_.push_back(model_id);
    
    spdlog::debug("Registered model: {} ({})", model->GetModelName(), model_id);
}

// Explicitly instantiate the template for the models we use
template void ModelFactory::RegisterModel<DeepseekR1Model>();
template void ModelFactory::RegisterModel<DeepseekV3Model>();

} // namespace ai_backend::services::ai