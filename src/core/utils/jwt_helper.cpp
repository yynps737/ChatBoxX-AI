#include "core/utils/jwt_helper.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ai_backend::core::utils {

// 工具函数：Base64 URL 编码
std::string JwtHelper::Base64UrlEncode(const std::vector<uint8_t>& input) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input.data(), static_cast<int>(input.size()));
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    // 转换成URL安全的Base64
    for (auto& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }

    // 移除padding字符
    size_t pos = result.find('=');
    if (pos != std::string::npos) {
        result.erase(pos);
    }

    return result;
}

// 工具函数：Base64 URL 解码
std::vector<uint8_t> JwtHelper::Base64UrlDecode(const std::string& input) {
    // 添加回padding
    std::string paddedInput = input;
    switch (input.size() % 4) {
        case 0: break; // 不需要padding
        case 2: paddedInput += "=="; break;
        case 3: paddedInput += "="; break;
        default: throw std::runtime_error("Invalid base64url input");
    }

    // 转换回标准Base64
    for (auto& c : paddedInput) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }

    BIO *bio, *b64;
    bio = BIO_new_mem_buf(paddedInput.c_str(), -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> result(paddedInput.size());
    int decodedSize = BIO_read(bio, result.data(), static_cast<int>(result.size()));
    BIO_free_all(bio);

    result.resize(decodedSize);
    return result;
}

// 创建JWT令牌
std::string JwtHelper::CreateToken(const nlohmann::json& payload, const std::string& secret, 
                                 std::chrono::seconds expiration) {
    try {
        // 创建header
        nlohmann::json header = {
            {"alg", "HS256"},
            {"typ", "JWT"}
        };

        // 添加过期时间
        auto now = std::chrono::system_clock::now();
        auto exp = now + expiration;
        auto exp_time = std::chrono::system_clock::to_time_t(exp);
        auto iat_time = std::chrono::system_clock::to_time_t(now);

        nlohmann::json updatedPayload = payload;
        updatedPayload["exp"] = exp_time;
        updatedPayload["iat"] = iat_time;

        // 生成header和payload的base64编码
        std::string header_b64 = Base64UrlEncode(std::vector<uint8_t>(header.dump().begin(), header.dump().end()));
        std::string payload_b64 = Base64UrlEncode(std::vector<uint8_t>(updatedPayload.dump().begin(), updatedPayload.dump().end()));

        // 生成签名
        std::string signature_data = header_b64 + "." + payload_b64;
        std::vector<uint8_t> signature = ComputeHmacSha256(signature_data, secret);
        std::string signature_b64 = Base64UrlEncode(signature);

        // 组合JWT
        return header_b64 + "." + payload_b64 + "." + signature_b64;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create JWT token: {}", e.what());
        return "";
    }
}

// 验证JWT令牌
bool JwtHelper::VerifyToken(const std::string& token, const std::string& secret) {
    try {
        auto parts = SplitToken(token);
        if (parts.size() != 3) {
            return false;
        }

        std::string header_b64 = parts[0];
        std::string payload_b64 = parts[1];
        std::string provided_signature_b64 = parts[2];

        // 重新计算签名
        std::string signature_data = header_b64 + "." + payload_b64;
        std::vector<uint8_t> computed_signature = ComputeHmacSha256(signature_data, secret);
        std::string computed_signature_b64 = Base64UrlEncode(computed_signature);

        // 检查签名是否一致
        if (computed_signature_b64 != provided_signature_b64) {
            return false;
        }

        // 检查过期时间
        auto payload = GetTokenPayload(token);
        if (payload.contains("exp")) {
            std::time_t exp_time = payload["exp"].get<std::time_t>();
            std::time_t now = std::time(nullptr);
            if (now > exp_time) {
                return false; // 令牌已过期
            }
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to verify JWT token: {}", e.what());
        return false;
    }
}

// 获取JWT令牌的载荷部分
nlohmann::json JwtHelper::GetTokenPayload(const std::string& token) {
    try {
        auto parts = SplitToken(token);
        if (parts.size() != 3) {
            return nlohmann::json();
        }

        std::vector<uint8_t> decoded = Base64UrlDecode(parts[1]);
        std::string payload_str(decoded.begin(), decoded.end());
        return nlohmann::json::parse(payload_str);
    } catch (const std::exception& e) {
        spdlog::error("Failed to extract JWT payload: {}", e.what());
        return nlohmann::json();
    }
}

// 拆分JWT令牌为三部分
std::vector<std::string> JwtHelper::SplitToken(const std::string& token) {
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string part;
    
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }
    
    return parts;
}

// 计算HMAC-SHA256签名
std::vector<uint8_t> JwtHelper::ComputeHmacSha256(const std::string& data, const std::string& key) {
    std::vector<uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int result_len = 0;

    HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
         result.data(), &result_len);

    result.resize(result_len);
    return result;
}

} // namespace ai_backend::core::utils