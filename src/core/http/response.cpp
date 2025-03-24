#include "core/http/response.h"

namespace ai_backend::core::http {

Response Response::WithJson(int status_code, const nlohmann::json& json_data) {
    Response response;
    response.status_code = status_code;
    response.headers["Content-Type"] = "application/json";
    response.body = json_data.dump();
    return response;
}

Response& Response::SetHeader(const std::string& name, const std::string& value) {
    headers[name] = value;
    return *this;
}

Response Response::OK(const nlohmann::json& data) {
    return WithJson(200, {
        {"code", 0},
        {"message", "成功"},
        {"data", data}
    });
}

Response Response::Created(const nlohmann::json& data) {
    return WithJson(201, {
        {"code", 0},
        {"message", "创建成功"},
        {"data", data}
    });
}

Response Response::BadRequest(const nlohmann::json& error) {
    return WithJson(400, {
        {"code", error.value("code", 400)},
        {"message", error.value("message", "请求参数错误")},
        {"data", error.value("data", nullptr)}
    });
}

Response Response::Unauthorized(const nlohmann::json& error) {
    return WithJson(401, {
        {"code", error.value("code", 401)},
        {"message", error.value("message", "未授权访问")},
        {"data", error.value("data", nullptr)}
    });
}

Response Response::Forbidden(const nlohmann::json& error) {
    return WithJson(403, {
        {"code", error.value("code", 403)},
        {"message", error.value("message", "禁止访问")},
        {"data", error.value("data", nullptr)}
    });
}

Response Response::NotFound(const nlohmann::json& error) {
    return WithJson(404, {
        {"code", error.value("code", 404)},
        {"message", error.value("message", "资源不存在")},
        {"data", error.value("data", nullptr)}
    });
}

Response Response::InternalServerError(const nlohmann::json& error) {
    return WithJson(500, {
        {"code", error.value("code", 500)},
        {"message", error.value("message", "服务器内部错误")},
        {"data", error.value("data", nullptr)}
    });
}

} // namespace ai_backend::core::http