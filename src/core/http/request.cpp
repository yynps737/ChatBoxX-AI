#include "core/http/request.h"

namespace ai_backend::core::http {

std::string Request::GetHeader(const std::string& name) const {
    auto it = headers.find(name);
    if (it != headers.end()) {
        return it->second;
    }
    return "";
}

std::string Request::GetQueryParam(const std::string& name, const std::string& default_value) const {
    auto it = query_params.find(name);
    if (it != query_params.end()) {
        return it->second;
    }
    return default_value;
}

std::string Request::GetPathParam(const std::string& name, const std::string& default_value) const {
    auto it = path_params.find(name);
    if (it != path_params.end()) {
        return it->second;
    }
    return default_value;
}

bool Request::HasHeader(const std::string& name) const {
    return headers.find(name) != headers.end();
}

bool Request::HasQueryParam(const std::string& name) const {
    return query_params.find(name) != query_params.end();
}

bool Request::HasPathParam(const std::string& name) const {
    return path_params.find(name) != path_params.end();
}

} // namespace ai_backend::core::http