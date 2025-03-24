#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace ai_backend::core::utils {

class StringUtils {
public:
    static std::vector<std::string> Split(const std::string& str, char delimiter);
    static std::vector<std::string> Split(const std::string& str, const std::string& delimiter);
    static std::string Join(const std::vector<std::string>& parts, const std::string& delimiter);
    
    static std::string ToLower(const std::string& str);
    static std::string ToUpper(const std::string& str);
    
    static std::string Trim(const std::string& str);
    static std::string TrimLeft(const std::string& str);
    static std::string TrimRight(const std::string& str);
    
    static std::string Replace(const std::string& str, const std::string& from, const std::string& to);
    
    static bool StartsWith(const std::string& str, const std::string& prefix);
    static bool EndsWith(const std::string& str, const std::string& suffix);
    
    static std::string UrlEncode(const std::string& str);
    static std::string UrlDecode(const std::string& str);
    
    static std::string HtmlEscape(const std::string& str);
    
    static std::string GenerateRandomString(size_t length);
    
    static std::string FormatBytes(uint64_t bytes);
    static std::string FormatDuration(std::chrono::milliseconds duration);
    
    static std::string Base64Encode(const unsigned char* data, size_t length);
    static std::vector<unsigned char> Base64Decode(const std::string& encoded);
};

} // namespace ai_backend::core::utils