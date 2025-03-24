#include "core/utils/string_utils.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>
#include <random>
#include <cctype>

namespace ai_backend::core::utils {

std::vector<std::string> StringUtils::Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

std::vector<std::string> StringUtils::Split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    
    tokens.push_back(str.substr(start));
    return tokens;
}

std::string StringUtils::Join(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::ostringstream result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result << delimiter;
        }
        result << parts[i];
    }
    return result.str();
}

// 其他 StringUtils 方法实现...

} // namespace ai_backend::core::utils
