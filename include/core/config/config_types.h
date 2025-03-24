#pragma once

#include <string>
#include <variant>
#include <vector>

namespace ai_backend::core::config {

using ConfigValue = std::variant
    std::string,
    int,
    double,
    bool,
    std::vector<std::string>,
    std::vector<int>,
    std::vector<double>
>;

} // namespace ai_backend::core::config