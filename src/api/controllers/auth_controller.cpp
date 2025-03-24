#include "api/controllers/auth_controller.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai_backend::api::controllers {

using json = nlohmann::json;
using namespace core::async;
using namespace core::http;

AuthController::AuthController(std::shared_ptr<services::auth::AuthService> auth_service)
    : auth_service_(std::move(auth_service)) {
}

Task<Response> AuthController::Register(const Request& request) {
    try {
        // 解析请求体JSON
        json request_body = json::parse(request.body);
        
        // 验证请求体
        if (!request_body.contains("username") || !request_body["username"].is_string() ||
            !request_body.contains("password") || !request_body["password"].is_string() ||
            !request_body.contains("email") || !request_body["email"].is_string()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", "用户名、密码和邮箱不能为空"},
                {"data", nullptr}
            });
        }
        
        std::string username = request_body["username"].get<std::string>();
        std::string password = request_body["password"].get<std::string>();
        std::string email = request_body["email"].get<std::string>();
        
        // 调用认证服务进行注册
        auto result = co_await auth_service_->Register(username, password, email);
        
        if (result.IsError()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 生成JWT令牌
        auto token_result = co_await auth_service_->GenerateToken(result.GetValue());
        
        if (token_result.IsError()) {
            co_return Response::InternalServerError({
                {"code", 500},
                {"message", token_result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 返回成功响应
        co_return Response::Created({
            {"code", 0},
            {"message", "注册成功"},
            {"data", {
                {"user_id", result.GetValue()},
                {"username", username},
                {"token", token_result.GetValue()}
            }}
        });
        
    } catch (const json::exception& e) {
        spdlog::error("JSON error in Register: {}", e.what());
        co_return Response::BadRequest({
            {"code", 400},
            {"message", "请求格式错误"},
            {"data", nullptr}
        });
    } catch (const std::exception& e) {
        spdlog::error("Error in Register: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> AuthController::Login(const Request& request) {
    try {
        // 解析请求体JSON
        json request_body = json::parse(request.body);
        
        // 验证请求体
        if (!request_body.contains("username") || !request_body["username"].is_string() ||
            !request_body.contains("password") || !request_body["password"].is_string()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", "用户名和密码不能为空"},
                {"data", nullptr}
            });
        }
        
        std::string username = request_body["username"].get<std::string>();
        std::string password = request_body["password"].get<std::string>();
        
        // 调用认证服务进行登录
        auto result = co_await auth_service_->Login(username, password);
        
        if (result.IsError()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 返回成功响应
        auto [access_token, refresh_token] = result.GetValue();
        
        co_return Response::OK({
            {"code", 0},
            {"message", "登录成功"},
            {"data", {
                {"access_token", access_token},
                {"refresh_token", refresh_token},
                {"expires_in", 3600} // 令牌过期时间(秒)
            }}
        });
        
    } catch (const json::exception& e) {
        spdlog::error("JSON error in Login: {}", e.what());
        co_return Response::BadRequest({
            {"code", 400},
            {"message", "请求格式错误"},
            {"data", nullptr}
        });
    } catch (const std::exception& e) {
        spdlog::error("Error in Login: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

Task<Response> AuthController::RefreshToken(const Request& request) {
    try {
        // 解析请求体JSON
        json request_body = json::parse(request.body);
        
        // 验证请求体
        if (!request_body.contains("refresh_token") || !request_body["refresh_token"].is_string()) {
            co_return Response::BadRequest({
                {"code", 400},
                {"message", "刷新令牌不能为空"},
                {"data", nullptr}
            });
        }
        
        std::string refresh_token = request_body["refresh_token"].get<std::string>();
        
        // 调用认证服务刷新令牌
        auto result = co_await auth_service_->RefreshToken(refresh_token);
        
        if (result.IsError()) {
            co_return Response::Unauthorized({
                {"code", 401},
                {"message", result.GetError()},
                {"data", nullptr}
            });
        }
        
        // 返回成功响应
        co_return Response::OK({
            {"code", 0},
            {"message", "刷新成功"},
            {"data", {
                {"access_token", result.GetValue()},
                {"expires_in", 3600} // 令牌过期时间(秒)
            }}
        });
        
    } catch (const json::exception& e) {
        spdlog::error("JSON error in RefreshToken: {}", e.what());
        co_return Response::BadRequest({
            {"code", 400},
            {"message", "请求格式错误"},
            {"data", nullptr}
        });
    } catch (const std::exception& e) {
        spdlog::error("Error in RefreshToken: {}", e.what());
        co_return Response::InternalServerError({
            {"code", 500},
            {"message", "服务器内部错误"},
            {"data", nullptr}
        });
    }
}

} // namespace ai_backend::api::controllers