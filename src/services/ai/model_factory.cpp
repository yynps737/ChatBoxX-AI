#include "services/ai/model_factory.h"
#include "services/ai/models/wenxin_model.h"
#include "services/ai/models/xunfei_model.h"
#include "services/ai/models/tongyi_model.h"
#include "services/ai/models/deepseek_r1_model.h"
#include "services/ai/models/deepseek_v3_model.h"

namespace ai_backend::services::ai {

ModelFactory& ModelFactory::GetInstance() {
    static ModelFactory instance;
    return instance;
}

std::shared_ptr<ModelInterface> ModelFactory::CreateModel(const std::string& model_id) {
    auto it = model_creators_.find(model_id);
    if (it == model_creators_.end()) {
        return nullptr;
    }
    return it->second();
}

const std::vector<std::string>& ModelFactory::GetAvailableModels() const {
    return available_models_;
}

void ModelFactory::Initialize() {
    // 注册所有支持的模型
    RegisterModel<WenxinModel>();
    RegisterModel<XunfeiModel>();
    RegisterModel<TongyiModel>();
    RegisterModel<DeepseekR1Model>();
    RegisterModel<DeepseekV3Model>();
}

} // namespace ai_backend::services::ai