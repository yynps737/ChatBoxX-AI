#pragma once

#include <string>
#include <vector>
#include "core/http/request.h"
#include "core/async/task.h"

namespace ai_backend::api::middlewares {

class CorsMiddleware {
public:
    CorsMiddleware();
    
    core::async::Task<bool> Process(core::http::Request& request);

private:
    std::vector<std::string> allowed_origins_;
    std::vector<std::string> allowed_methods_;
    std::vector<std::string> allowed_headers_;
    bool allow_credentials_;
    int max_age_;
};

} // namespace ai_backend::api::middlewares