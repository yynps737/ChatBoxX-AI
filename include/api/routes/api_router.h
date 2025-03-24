#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/http/router.h"
#include "core/http/request.h"
#include "core/http/response.h"
#include "api/controllers/auth_controller.h"
#include "api/controllers/dialog_controller.h"
#include "api/controllers/message_controller.h"
#include "api/controllers/file_controller.h"
#include "api/controllers/model_controller.h"
#include "api/middlewares/auth_middleware.h"
#include "api/middlewares/cors_middleware.h"
#include "api/middlewares/rate_limiter.h"
#include "api/middlewares/request_logger.h"

namespace ai_backend::api::routes {

// API路由器类，定义所有API路由
class ApiRouter : public core::http::Router {
public:
    ApiRouter();
    
    // 初始化路由
    void Initialize() override;
    
    // 路由请求
    core::async::Task<core::http::Response> Route(const core::http::Request& request) override;

private:
    // 设置中间件
    void SetupMiddlewares();
    
    // 设置路由
    void SetupRoutes();
    
    // 创建控制器
    void CreateControllers();
    
    // 添加路由
    void AddRoute(const std::string& path, 
                 const std::string& method, 
                 std::function<core::async::Task<core::http::Response>(const core::http::Request&)> handler,
                 bool require_auth = true);

private:
    // 中间件集合
    std::vector<std::function<core::async::Task<bool>(core::http::Request&)>> middlewares_;
    
    // 路由表，key为"方法:路径"
    struct RouteEntry {
        std::function<core::async::Task<core::http::Response>(const core::http::Request&)> handler;
        bool require_auth;
    };
    std::unordered_map<std::string, RouteEntry> routes_;
    
    // 路径参数正则表达式
    std::vector<std::pair<std::regex, std::string>> path_params_regexes_;
    
    // 控制器
    std::shared_ptr<controllers::AuthController> auth_controller_;
    std::shared_ptr<controllers::DialogController> dialog_controller_;
    std::shared_ptr<controllers::MessageController> message_controller_;
    std::shared_ptr<controllers::FileController> file_controller_;
    std::shared_ptr<controllers::ModelController> model_controller_;
    
    // 中间件
    std::shared_ptr<middlewares::AuthMiddleware> auth_middleware_;
    std::shared_ptr<middlewares::CorsMiddleware> cors_middleware_;
    std::shared_ptr<middlewares::RateLimiter> rate_limiter_;
    std::shared_ptr<middlewares::RequestLogger> request_logger_;
};

} // namespace ai_backend::api::routes