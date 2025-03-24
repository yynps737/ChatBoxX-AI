#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "core/http/request.h"
#include "core/http/response.h"
#include "core/async/task.h"

namespace ai_backend::core::http {

class Router {
public:
    virtual ~Router() = default;
    
    virtual void Initialize() = 0;
    virtual async::Task<Response> Route(const Request& request) = 0;
};

} // namespace ai_backend::core::http