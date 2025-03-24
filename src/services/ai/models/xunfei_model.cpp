#include "services/ai/models/xunfei_model.h"
#include "core/config/config_manager.h"
#include "core/http/request.h"
#include "core/utils/string_utils.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <ctime>
#include <chrono>

namespace ai_backend::services::ai {

using json = nlohmann::json;
using namespace core::async;

XunfeiModel::XunfeiModel() 
    : last_prompt_tokens_(0), 
      last_completion_tokens_(0),
      is_healthy_(true) {
    
    auto& config = core::config::ConfigManager::GetInstance();
    app_id_ = config.GetString("ai.xunfei.app_id", "");
    api_key_ = config.GetString("ai.xunfei.api_key", "");
    api_secret_ = config.GetString("ai.xunfei.api_secret", "");
    api_base_url_ = config.GetString("ai.xunfei.base_url", "wss://spark-api.xf-yun.com/v2.1/chat");
    
    if (app_id_.empty() || api_key_.empty() || api_secret_.empty()) {
        spdlog::error("Xunfei API credentials not configured");
        is_healthy_ = false;
    }
}

XunfeiModel::~XunfeiModel() = default;

std::string XunfeiModel::GetModelId() const {
    return MODEL_ID;
}

std::string XunfeiModel::GetModelName() const {
    return MODEL_NAME;
}

std::string XunfeiModel::GetModelProvider() const {
    return MODEL_PROVIDER;
}

std::vector<std::string> XunfeiModel::GetCapabilities() const {
    return {
        "text_generation", 
        "reasoning", 
        "creative_writing",
        "summarization",
        "translation"
    };
}

bool XunfeiModel::SupportsStreaming() const {
    return true;
}

Task<common::Result<std::string>> 
XunfeiModel::GenerateResponse(const std::vector<models::Message>& messages, 
                            const ModelConfig& config) {
    try {
        std::string auth_url = GenerateAuthUrl();
        json request_body = BuildRequestBody(messages, config);
        
        // 发送WebSocket请求
        WebSocketClient ws_client;
        auto connect_result = co_await ws_client.Connect(auth_url);
        
        if (!connect_result) {
            co_return common::Result<std::string>::Error("Failed to connect to Xunfei API");
        }
        
        auto send_result = co_await ws_client.Send(request_body.dump());
        if (!send_result) {
            co_return common::Result<std::string>::Error("Failed to send request to Xunfei API");
        }
        
        std::string full_response;
        bool is_done = false;
        
        while (!is_done) {
            auto response = co_await ws_client.Receive();
            if (response.empty()) {
                break;
            }
            
            try {
                auto json_response = json::parse(response);
                
                if (json_response.contains("header") && 
                    json_response["header"].contains("code") && 
                    json_response["header"]["code"] != 0) {
                    
                    std::string error_msg = "Xunfei API error: ";
                    if (json_response["header"].contains("message")) {
                        error_msg += json_response["header"]["message"].get<std::string>();
                    } else {
                        error_msg += "Unknown error";
                    }
                    
                    co_return common::Result<std::string>::Error(error_msg);
                }
                
                if (json_response.contains("payload") && 
                    json_response["payload"].contains("choices") && 
                    !json_response["payload"]["choices"].empty() && 
                    json_response["payload"]["choices"][0].contains("text")) {
                    
                    auto& text = json_response["payload"]["choices"][0]["text"];
                    if (text.contains("content")) {
                        full_response += text["content"].get<std::string>();
                    }
                }
                
                if (json_response.contains("header") && 
                    json_response["header"].contains("status") && 
                    json_response["header"]["status"] == 2) {
                    is_done = true;
                    
                    // 获取token计数
                    if (json_response.contains("payload") && 
                        json_response["payload"].contains("usage") && 
                        json_response["payload"]["usage"].contains("text")) {
                        
                        auto& usage = json_response["payload"]["usage"]["text"];
                        
                        if (usage.contains("prompt_tokens")) {
                            last_prompt_tokens_ = usage["prompt_tokens"].get<size_t>();
                        }
                        
                        if (usage.contains("completion_tokens")) {
                            last_completion_tokens_ = usage["completion_tokens"].get<size_t>();
                        }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Error parsing Xunfei response: {}", e.what());
            }
        }
        
        ws_client.Close();
        
        if (full_response.empty()) {
            co_return common::Result<std::string>::Error("Empty response from Xunfei API");
        }
        
        co_return common::Result<std::string>::Ok(full_response);
    } catch (const std::exception& e) {
        std::string error_msg = "Exception in XunfeiModel::GenerateResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        co_return common::Result<std::string>::Error(error_msg);
    }
}

Task<common::Result<void>> 
XunfeiModel::GenerateStreamingResponse(const std::vector<models::Message>& messages,
                                     StreamCallback callback,
                                     const ModelConfig& config) {
    try {
        std::string auth_url = GenerateAuthUrl();
        json request_body = BuildRequestBody(messages, config);
        
        // 发送WebSocket请求
        WebSocketClient ws_client;
        auto connect_result = co_await ws_client.Connect(auth_url);
        
        if (!connect_result) {
            callback("", true);
            co_return common::Result<void>::Error("Failed to connect to Xunfei API");
        }
        
        auto send_result = co_await ws_client.Send(request_body.dump());
        if (!send_result) {
            callback("", true);
            co_return common::Result<void>::Error("Failed to send request to Xunfei API");
        }
        
        size_t tokens = 0;
        bool is_done = false;
        
        while (!is_done) {
            auto response = co_await ws_client.Receive();
            if (response.empty()) {
                break;
            }
            
            try {
                auto json_response = json::parse(response);
                
                if (json_response.contains("header") && 
                    json_response["header"].contains("code") && 
                    json_response["header"]["code"] != 0) {
                    
                    std::string error_msg = "Xunfei API error: ";
                    if (json_response["header"].contains("message")) {
                        error_msg += json_response["header"]["message"].get<std::string>();
                    } else {
                        error_msg += "Unknown error";
                    }
                    
                    callback("", true);
                    co_return common::Result<void>::Error(error_msg);
                }
                
                if (json_response.contains("payload") && 
                    json_response["payload"].contains("choices") && 
                    !json_response["payload"]["choices"].empty() && 
                    json_response["payload"]["choices"][0].contains("text")) {
                    
                    auto& text = json_response["payload"]["choices"][0]["text"];
                    if (text.contains("content")) {
                        std::string content = text["content"].get<std::string>();
                        callback(content, false);
                        tokens++;
                    }
                }
                
                if (json_response.contains("header") && 
                    json_response["header"].contains("status") && 
                    json_response["header"]["status"] == 2) {
                    is_done = true;
                    callback("", true);
                    
                    // 获取token计数
                    if (json_response.contains("payload") && 
                        json_response["payload"].contains("usage") && 
                        json_response["payload"]["usage"].contains("text")) {
                        
                        auto& usage = json_response["payload"]["usage"]["text"];
                        
                        if (usage.contains("prompt_tokens")) {
                            last_prompt_tokens_ = usage["prompt_tokens"].get<size_t>();
                        }
                        
                        if (usage.contains("completion_tokens")) {
                            last_completion_tokens_ = usage["completion_tokens"].get<size_t>();
                        } else {
                            last_completion_tokens_ = tokens;
                        }
                    } else {
                        last_completion_tokens_ = tokens;
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Error parsing Xunfei stream response: {}", e.what());
            }
        }
        
        ws_client.Close();
        
        if (!is_done) {
            callback("", true);
        }
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        std::string error_msg = "Exception in XunfeiModel::GenerateStreamingResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        callback("", true);
        co_return common::Result<void>::Error(error_msg);
    }
}

size_t XunfeiModel::GetLastPromptTokens() const {
    return last_prompt_tokens_;
}

size_t XunfeiModel::GetLastCompletionTokens() const {
    return last_completion_tokens_;
}

size_t XunfeiModel::GetLastTotalTokens() const {
    return last_prompt_tokens_ + last_completion_tokens_;
}

bool XunfeiModel::IsHealthy() const {
    return is_healthy_;
}

void XunfeiModel::Reset() {
    last_prompt_tokens_ = 0;
    last_completion_tokens_ = 0;
}

std::string XunfeiModel::GenerateAuthUrl() const {
    // 当前时间
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    // RFC1123格式的日期
    char date_buf[30];
    std::strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&now_time_t));
    std::string date_str(date_buf);
    
    // 构建鉴权URL
    std::string auth_url = api_base_url_;
    
    // 解析host和path
    std::string host;
    std::string path;
    
    size_t host_start = api_base_url_.find("://");
    if (host_start != std::string::npos) {
        host_start += 3;
        size_t path_start = api_base_url_.find("/", host_start);
        if (path_start != std::string::npos) {
            host = api_base_url_.substr(host_start, path_start - host_start);
            path = api_base_url_.substr(path_start);
        } else {
            host = api_base_url_.substr(host_start);
            path = "/";
        }
    }
    
    // 构建签名原文
    std::string signature_origin = "host: " + host + "\n";
    signature_origin += "date: " + date_str + "\n";
    signature_origin += "GET " + path + " HTTP/1.1";
    
    // 计算签名
    unsigned char hmac_result[EVP_MAX_MD_SIZE];
    unsigned int hmac_len = 0;
    
    HMAC(EVP_sha256(), api_secret_.c_str(), static_cast<int>(api_secret_.size()),
         reinterpret_cast<const unsigned char*>(signature_origin.c_str()), signature_origin.size(),
         hmac_result, &hmac_len);
    
    // Base64编码
    std::string signature = core::utils::StringUtils::Base64Encode(hmac_result, hmac_len);
    
    // 构建Authorization
    std::string authorization = "api_key=\"" + api_key_ + "\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"" + signature + "\"";
    
    // 拼接URL参数
    auth_url += "?authorization=" + core::utils::StringUtils::UrlEncode(authorization);
    auth_url += "&date=" + core::utils::StringUtils::UrlEncode(date_str);
    auth_url += "&host=" + core::utils::StringUtils::UrlEncode(host);
    
    return auth_url;
}

json XunfeiModel::BuildRequestBody(const std::vector<models::Message>& messages, const ModelConfig& config) const {
    json request_body;
    
    // Header
    request_body["header"] = {
        {"app_id", app_id_},
        {"uid", "user"}
    };
    
    // Parameter
    request_body["parameter"] = {
        {"chat", {
            {"domain", "general"},
            {"temperature", config.temperature},
            {"top_k", 4},
            {"max_tokens", config.max_tokens},
            {"auditing", "default"}
        }}
    };
    
    // Payload
    json messages_json = json::array();
    for (const auto& message : messages) {
        json msg_json = {
            {"role", message.role},
            {"content", message.content}
        };
        messages_json.push_back(msg_json);
    }
    
    request_body["payload"] = {
        {"message", {
            {"text", messages_json}
        }}
    };
    
    return request_body;
}

// 简单WebSocket客户端
XunfeiModel::WebSocketClient::WebSocketClient() : is_connected_(false) {}

XunfeiModel::WebSocketClient::~WebSocketClient() {
    Close();
}

Task<bool> XunfeiModel::WebSocketClient::Connect(const std::string& url) {
    try {
        // 此处省略实际WebSocket连接实现
        // 在实际项目中，应使用如Boost.Beast等库实现WebSocket客户端
        // 此处仅提供框架代码
        is_connected_ = true;
        co_return true;
    } catch (const std::exception& e) {
        spdlog::error("Error connecting to WebSocket: {}", e.what());
        co_return false;
    }
}

Task<bool> XunfeiModel::WebSocketClient::Send(const std::string& message) {
    if (!is_connected_) {
        co_return false;
    }
    
    try {
        // 发送消息
        co_return true;
    } catch (const std::exception& e) {
        spdlog::error("Error sending WebSocket message: {}", e.what());
        co_return false;
    }
}

Task<std::string> XunfeiModel::WebSocketClient::Receive() {
    if (!is_connected_) {
        co_return "";
    }
    
    try {
        // 接收消息
        co_return ""; // 返回接收到的消息
    } catch (const std::exception& e) {
        spdlog::error("Error receiving WebSocket message: {}", e.what());
        co_return "";
    }
}

void XunfeiModel::WebSocketClient::Close() {
    if (is_connected_) {
        try {
            // 关闭连接
            is_connected_ = false;
        } catch (const std::exception& e) {
            spdlog::error("Error closing WebSocket connection: {}", e.what());
        }
    }
}

} // namespace ai_backend::services::ai