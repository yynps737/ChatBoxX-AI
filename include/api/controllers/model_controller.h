#pragma once

#include <memory>
#include <string>

#include "core/http/request.h"
#include "core/http/response.h"
#include "core/async/task.h"
#include "services/ai/model_service.h"

namespace ai_backend::api::controllers {

class ModelController {
public:
    ModelController();
    
    core::async::Task<core::http::Response> GetModels(const core::http::Request& request);
    core::async::Task<core::http::Response> GetModelById(const core::http::Request& request);

private:
    services::ai::ModelService& model_service_;
};

} // namespace ai_backend::api::controllers