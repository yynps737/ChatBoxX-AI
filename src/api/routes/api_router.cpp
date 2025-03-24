#include "api/routes/api_router.h"
#include <spdlog/spdlog.h>
#include <regex>

namespace ai_backend::api::routes {

using namespace core::async;
using namespace core::http;

ApiRouter::ApiRouter() {
    // 创建控制器和中间件
    CreateControllers();
    SetupMiddlewares();
    SetupRoutes();
}

void ApiRouter::Initialize() {
    // 已在构造函数中初始化
}

Task<Response> ApiRouter::Route(const Request& request) {
    try {
        // 创建请求的可变副本，以便中间件可以修改它
        Request mutable_request = request;
        
        // 应用所有中间件
        for (const auto& middleware : middlewares_) {
            bool should_continue = co_await middleware(mutable_request);
            if (!should_continue) {
                // 中间件请求终止处理
                co_return Response::Forbidden({
                    {"code", 403},
                    {"message", "请求被中间件拒绝"},
                    {"data", nullptr}
                });
            }
        }
        
        // 提取路径和方法
        std::string path = mutable_request.path;
        std::string method = mutable_request.method;
        
        // 尝试查找精确匹配的路由
        std::string route_key = method + ":" + path;
        auto route_it = routes_.find(route_key);
        
        if (route_it != routes_.end()) {
            // 找到精确匹配
            if (route_it->second.require_auth && !mutable_request.user_id.has_value()) {
                co_return Response::Unauthorized({
                    {"code", 401},
                    {"message", "请先登录"},
                    {"data", nullptr}
                });
            }
            
            // 调用路由处理器
            co_return co_await route_it->second.handler(mutable_request);
        }
        
        // 如果没有精确匹配，尝试使用路径参数匹配
        for (const auto& [regex, route_template] : path_params_regexes_) {
            std::smatch matches;
            if (std::regex_match(path, matches, regex)) {
                // 找到匹配的正则表达式路由
                std::string route_key = method + ":" + route_template;
                auto route_it = routes_.find(route_key);
                
                if (route_it != routes_.end()) {
                    if (route_it->second.require_auth && !mutable_request.user_id.has_value()) {
                        co_return Response::Unauthorized({
                            {"code", 401},
                            {"message", "请先登录"},
                            {"data", nullptr}
                        });
                    }
                    
                    // 提取路径参数
                    std::smatch param_matches;
                    std::regex param_regex(R"(\{([^{}]+)\})");
                    std::string route_template_copy = route_template;
                    std::string::const_iterator search_start = route_template_copy.cbegin();
                    
                    size_t param_index = 1; // 跳过第一个匹配（整个字符串）
                    while (std::regex_search(search_start, route_template_copy.cend(), param_matches, param_regex)) {
                        if (param_index < matches.size()) {
                            std::string param_name = param_matches[1].str();
                            std::string param_value = matches[param_index].str();
                            mutable_request.path_params[param_name] = param_value;
                        }
                        
                        search_start = param_matches.suffix().first;
                        param_index++;
                    }
                    
                    // 调用路由处理器
                    co_return co_await route_it->second.handler(mutable_request);
                }
            }
        }
        
        // 如果没有找到匹配的路由，返回404
        co_return Response::NotFound({
            {"code", 404},
            {"message", "API路径不存在"},
            {"data", nullptr}
        });
    } catch (const std::exception& e) {
        spdlog::error("Error in Route: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

void ApiRouter::SetupMiddlewares() {
    // 添加中间件，按执行顺序添加
    middlewares_.push_back([this](Request& req) -> Task<bool> {
        return co_await cors_middleware_->Process(req);
    });
    
    middlewares_.push_back([this](Request& req) -> Task<bool> {
        return co_await request_logger_->Process(req);
    });
    
    middlewares_.push_back([this](Request& req) -> Task<bool> {
        return co_await rate_limiter_->Process(req);
    });
    
    middlewares_.push_back([this](Request& req) -> Task<bool> {
        return co_await auth_middleware_->Process(req);
    });
}

void ApiRouter::SetupRoutes() {
    // 认证相关路由
    AddRoute("/api/v1/auth/register", "POST", 
        [this](const Request& req) { return auth_controller_->Register(req); }, false);
    
    AddRoute("/api/v1/auth/login", "POST", 
        [this](const Request& req) { return auth_controller_->Login(req); }, false);
    
    AddRoute("/api/v1/auth/refresh", "POST", 
        [this](const Request& req) { return auth_controller_->RefreshToken(req); }, true);
    
    // 对话相关路由
    AddRoute("/api/v1/dialogs", "GET", 
        [this](const Request& req) { return dialog_controller_->GetDialogs(req); }, true);
    
    AddRoute("/api/v1/dialogs", "POST", 
        [this](const Request& req) { return dialog_controller_->CreateDialog(req); }, true);
    
    AddRoute("/api/v1/dialogs/{id}", "GET", 
        [this](const Request& req) { return dialog_controller_->GetDialogById(req); }, true);
    
    AddRoute("/api/v1/dialogs/{id}", "PUT", 
        [this](const Request& req) { return dialog_controller_->UpdateDialog(req); }, true);
    
    AddRoute("/api/v1/dialogs/{id}", "DELETE", 
        [this](const Request& req) { return dialog_controller_->DeleteDialog(req); }, true);
    
    // 消息相关路由
    AddRoute("/api/v1/dialogs/{dialog_id}/messages", "GET", 
        [this](const Request& req) { return message_controller_->GetMessages(req); }, true);
    
    AddRoute("/api/v1/dialogs/{dialog_id}/messages", "POST", 
        [this](const Request& req) { return message_controller_->CreateMessage(req); }, true);
    
    AddRoute("/api/v1/dialogs/{dialog_id}/messages/{message_id}/reply", "GET", 
        [this](const Request& req) { return message_controller_->GetReply(req); }, true);
    
    AddRoute("/api/v1/dialogs/{dialog_id}/messages/{message_id}", "DELETE", 
        [this](const Request& req) { return message_controller_->DeleteMessage(req); }, true);
    
    // 文件相关路由
    AddRoute("/api/v1/files", "POST", 
        [this](const Request& req) { return file_controller_->UploadFile(req); }, true);
    
    AddRoute("/api/v1/files/{id}", "GET", 
        [this](const Request& req) { return file_controller_->GetFile(req); }, true);
    
    AddRoute("/api/v1/files/{id}", "DELETE", 
        [this](const Request& req) { return file_controller_->DeleteFile(req); }, true);
    
    // 模型相关路由
    AddRoute("/api/v1/models", "GET", 
        [this](const Request& req) { return model_controller_->GetModels(req); }, true);
    
    AddRoute("/api/v1/models/{id}", "GET", 
        [this](const Request& req) { return model_controller_->GetModelById(req); }, true);
}

void ApiRouter::CreateControllers() {
    // 创建服务实例（这里简化了，实际项目中应该使用依赖注入容器）
    auto auth_service = std::make_shared<services::auth::AuthService>();
    auto dialog_service = std::make_shared<services::dialog::DialogService>();
    auto message_service = std::make_shared<services::message::MessageService>();
    auto file_service = std::make_shared<services::file::FileService>();
    
    // 创建控制器
    auth_controller_ = std::make_shared<controllers::AuthController>(auth_service);
    dialog_controller_ = std::make_shared<controllers::DialogController>(dialog_service);
    message_controller_ = std::make_shared<controllers::MessageController>(message_service, dialog_service);
    file_controller_ = std::make_shared<controllers::FileController>(file_service);
    model_controller_ = std::make_shared<controllers::ModelController>();
    
    // 创建中间件
    auth_middleware_ = std::make_shared<middlewares::AuthMiddleware>(auth_service);
    cors_middleware_ = std::make_shared<middlewares::CorsMiddleware>();
    rate_limiter_ = std::make_shared<middlewares::RateLimiter>();
    request_logger_ = std::make_shared<middlewares::RequestLogger>();
}

void ApiRouter::AddRoute(
    const std::string& path, 
    const std::string& method, 
    std::function<Task<Response>(const Request&)> handler,
    bool require_auth
) {
    std::string route_key = method + ":" + path;
    routes_[route_key] = {handler, require_auth};
    
    // 如果路径包含参数，添加到正则表达式列表
    if (path.find('{') != std::string::npos && path.find('}') != std::string::npos) {
        // 将路径模板转换为正则表达式
        std::string regex_str = path;
        std::regex param_regex(R"(\{([^{}]+)\})");
        regex_str = std::regex_replace(regex_str, param_regex, "([^/]+)");
        
        // 转义正则表达式中的特殊字符
        regex_str = std::regex_replace(regex_str, std::regex(R"([.+*?^$()[\]{}|\\])"), "\\$&");
        
        // 存储正则表达式和原始模板
        path_params_regexes_.emplace_back(std::regex(regex_str), path);
    }
}

} // namespace ai_backend::api::routes