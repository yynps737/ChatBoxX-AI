#include "api/controllers/model_controller.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "services/ai/model_service.h"

namespace ai_backend::api::controllers {

using json = nlohmann::json;
using namespace core::async;
using namespace core::http;

ModelController::ModelController() 
    : model_service_(services::ai::ModelService::GetInstance()) {
}

Task<Response> ModelController::GetModels(const Request& request) {
    try {
        // 获取所有模型信息
        auto models = model_service_.GetAllModelsInfo();
        
        // 转换结果为JSON
        json models_json = json::array();
        for (const auto& model : models) {
            json capabilities_json = json::array();
            for (const auto& capability : model.capabilities) {
                capabilities_json.push_back(capability);
            }
            
            models_json.push_back({
                {"id", model.id},
                {"name", model.name},
                {"provider", model.provider},
                {"capabilities", capabilities_json},
                {"supports_streaming", model.supports_streaming}
            });
        }
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "获取成功"},
            {"data", {
                {"models", models_json}
            }}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetModels: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> ModelController::GetModelById(const Request& request) {
    try {
        // 获取模型ID
        std::string model_id = request.GetPathParam("id");
        
        // 获取模型信息
        auto result = model_service_.GetModelInfo(model_id);
        
        if (result.IsError()) {
            co_return Response::NotFound({
                {"code", 404},
                {"message", "模型不存在"},
                {"data", nullptr}
            });
        }
        
        const auto& model = result.GetValue();
        
        // 转换能力列表为JSON
        json capabilities_json = json::array();
        for (const auto& capability : model.capabilities) {
            capabilities_json.push_back(capability);
        }
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "获取成功"},
            {"data", {
                {"id", model.id},
                {"name", model.name},
                {"provider", model.provider},
                {"capabilities", capabilities_json},
                {"supports_streaming", model.supports_streaming}
            }}
        });
        
    } catch (const std::exception& e) {
        spdlog::error("Error in GetModelById: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

} // namespace ai_backend::api::controllers