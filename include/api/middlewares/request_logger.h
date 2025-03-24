#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "core/http/request.h"
#include "core/http/response.h"
#include "core/async/task.h"
#include "core/config/config_manager.h"

namespace ai_backend::api::middlewares {

class RequestLogger {
public:
    RequestLogger();
    
    core::async::Task<bool> Process(core::http::Request& request);
    void LogResponse(const core::http::Request& request, const core::http::Response& response, std::chrono::milliseconds duration);

private:
    void LogRequest(const core::http::Request& request);
    std::string GenerateRequestId();
    void SanitizeJsonObject(nlohmann::json& json);

private:
    std::string log_level_;
    bool log_body_;
    bool log_headers_;
};

} // namespace ai_backend::api::middlewares