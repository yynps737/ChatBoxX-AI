#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

namespace ai_backend::core::utils {

class JwtHelper {
public:
    static std::string CreateToken(const nlohmann::json& payload, 
                                 const std::string& secret, 
                                 std::chrono::seconds expiration = std::chrono::hours(24));
    
    static bool VerifyToken(const std::string& token, const std::string& secret);
    static nlohmann::json GetTokenPayload(const std::string& token);
    
private:
    static std::string Base64UrlEncode(const std::vector<uint8_t>& input);
    static std::vector<uint8_t> Base64UrlDecode(const std::string& input);
    static std::vector<std::string> SplitToken(const std::string& token);
    static std::vector<uint8_t> ComputeHmacSha256(const std::string& data, const std::string& key);
};

} // namespace ai_backend::core::utils