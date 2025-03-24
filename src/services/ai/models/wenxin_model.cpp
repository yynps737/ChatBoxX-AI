#include "services/ai/models/wenxin_model.h"
#include "core/config/config_manager.h"
#include "core/http/request.h"
#include "core/utils/string_utils.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai_backend::services::ai {

using json = nlohmann::json;
using namespace core::async;

WenxinModel::WenxinModel() 
    : last_prompt_tokens_(0), 
      last_completion_tokens_(0),
      is_healthy_(true) {
    
    auto& config = core::config::ConfigManager::GetInstance();
    api_key_ = config.GetString("ai.wenxin.api_key", "");
    api_secret_ = config.GetString("ai.wenxin.api_secret", "");
    api_base_url_ = config.GetString("ai.wenxin.base_url", "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat");
    
    if (api_key_.empty() || api_secret_.empty()) {
        spdlog::error("Wenxin API credentials not configured");
        is_healthy_ = false;
    } else {
        RefreshAccessToken();
    }
}

WenxinModel::~WenxinModel() = default;

std::string WenxinModel::GetModelId() const {
    return MODEL_ID;
}

std::string WenxinModel::GetModelName() const {
    return MODEL_NAME;
}

std::string WenxinModel::GetModelProvider() const {
    return MODEL_PROVIDER;
}

std::vector<std::string> WenxinModel::GetCapabilities() const {
    return {
        "text_generation", 
        "reasoning", 
        "creative_writing",
        "summarization",
        "translation"
    };
}

bool WenxinModel::SupportsStreaming() const {
    return true;
}

Task<common::Result<std::string>> 
WenxinModel::GenerateResponse(const std::vector<models::Message>& messages, 
                           const ModelConfig& config) {
    try {
        RefreshAccessTokenIfNeeded();
        
        auto request = BuildAPIRequest(messages, config, false);
        
        auto response = co_await request.SendAsync();
        
        if (response.status_code != 200) {
            std::string error_msg = "Wenxin API error: " + 
                                  std::to_string(response.status_code) + " " + 
                                  response.body;
            spdlog::error(error_msg);
            co_return common::Result<std::string>::Error(error_msg);
        }
        
        auto result = ParseAPIResponse(response.body);
        
        if (result.IsOk()) {
            CalculateAndStoreTokenCounts(messages, result.GetValue());
        }
        
        co_return result;
    } catch (const std::exception& e) {
        std::string error_msg = "Exception in WenxinModel::GenerateResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        co_return common::Result<std::string>::Error(error_msg);
    }
}

Task<common::Result<void>> 
WenxinModel::GenerateStreamingResponse(const std::vector<models::Message>& messages,
                                     StreamCallback callback,
                                     const ModelConfig& config) {
    try {
        RefreshAccessTokenIfNeeded();
        
        auto request = BuildAPIRequest(messages, config, true);
        
        bool is_done = false;
        size_t tokens = 0;
        
        auto stream_handler = [this, &callback, &is_done, &tokens]
                              (const std::string& chunk) {
            this->HandleStreamChunk(chunk, callback, is_done);
            tokens++;
        };
        
        auto response = co_await request.SendStreamAsync(stream_handler);
        
        if (response.status_code != 200) {
            std::string error_msg = "Wenxin API streaming error: " + 
                                  std::to_string(response.status_code);
            spdlog::error(error_msg);
            callback("", true);
            co_return common::Result<void>::Error(error_msg);
        }
        
        if (!is_done) {
            callback("", true);
        }
        
        last_completion_tokens_ = tokens;
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        std::string error_msg = "Exception in WenxinModel::GenerateStreamingResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        callback("", true);
        co_return common::Result<void>::Error(error_msg);
    }
}

size_t WenxinModel::GetLastPromptTokens() const {
    return last_prompt_tokens_;
}

size_t WenxinModel::GetLastCompletionTokens() const {
    return last_completion_tokens_;
}

size_t WenxinModel::GetLastTotalTokens() const {
    return last_prompt_tokens_ + last_completion_tokens_;
}

bool WenxinModel::IsHealthy() const {
    return is_healthy_;
}

void WenxinModel::Reset() {
    last_prompt_tokens_ = 0;
    last_completion_tokens_ = 0;
}

void WenxinModel::RefreshAccessToken() {
    try {
        core::http::Request token_request;
        token_request.url = "https://aip.baidubce.com/oauth/2.0/token";
        token_request.method = "POST";
        token_request.headers = {
            {"Content-Type", "application/x-www-form-urlencoded"}
        };
        
        std::stringstream body_ss;
        body_ss << "grant_type=client_credentials&client_id=" << api_key_
                << "&client_secret=" << api_secret_;
        token_request.body = body_ss.str();
        
        auto response = token_request.SendSync();
        
        if (response.status_code != 200) {
            spdlog::error("Failed to refresh Wenxin access token: {}", response.body);
            is_healthy_ = false;
            return;
        }
        
        auto json_response = json::parse(response.body);
        
        if (!json_response.contains("access_token")) {
            spdlog::error("Invalid Wenxin token response: {}", response.body);
            is_healthy_ = false;
            return;
        }
        
        access_token_ = json_response["access_token"].get<std::string>();
        
        if (json_response.contains("expires_in")) {
            token_expiry_ = std::chrono::system_clock::now() + 
                         std::chrono::seconds(json_response["expires_in"].get<int>() - 60);
        } else {
            token_expiry_ = std::chrono::system_clock::now() + std::chrono::hours(23);
        }
        
        is_healthy_ = true;
    } catch (const std::exception& e) {
        spdlog::error("Exception in refreshing access token: {}", e.what());
        is_healthy_ = false;
    }
}

void WenxinModel::RefreshAccessTokenIfNeeded() {
    auto now = std::chrono::system_clock::now();
    if (access_token_.empty() || now >= token_expiry_) {
        RefreshAccessToken();
    }
}

core::http::Request 
WenxinModel::BuildAPIRequest(const std::vector<models::Message>& messages,
                          const ModelConfig& config,
                          bool stream) const {
    json request_body;
    
    json messages_json = json::array();
    for (const auto& message : messages) {
        json msg_json;
        msg_json["role"] = message.role;
        msg_json["content"] = message.content;
        messages_json.push_back(msg_json);
    }
    
    request_body["messages"] = messages_json;
    request_body["stream"] = stream;
    
    request_body["temperature"] = config.temperature;
    request_body["top_p"] = config.top_p;
    
    if (config.max_tokens > 0) {
        request_body["max_tokens"] = config.max_tokens;
    }
    
    if (!config.stop_sequences.empty()) {
        request_body["stop"] = config.stop_sequences;
    }
    
    for (const auto& [key, value] : config.additional_params) {
        request_body[key] = value;
    }
    
    core::http::Request request;
    request.url = api_base_url_ + "/ernie-bot-4?access_token=" + access_token_;
    request.method = "POST";
    request.headers = {
        {"Content-Type", "application/json"}
    };
    request.body = request_body.dump();
    
    return request;
}

common::Result<std::string> 
WenxinModel::ParseAPIResponse(const std::string& response) {
    try {
        auto json_response = json::parse(response);
        
        if (json_response.contains("error_code")) {
            std::string error_msg = "Wenxin API error: ";
            if (json_response.contains("error_msg")) {
                error_msg += json_response["error_msg"].get<std::string>();
            } else {
                error_msg += "Unknown error";
            }
            return common::Result<std::string>::Error(error_msg);
        }
        
        if (!json_response.contains("result")) {
            return common::Result<std::string>::Error("Invalid response format from Wenxin API");
        }
        
        std::string content = json_response["result"].get<std::string>();
        
        if (json_response.contains("usage")) {
            auto& usage = json_response["usage"];
            if (usage.contains("prompt_tokens")) {
                last_prompt_tokens_ = usage["prompt_tokens"].get<size_t>();
            }
            if (usage.contains("completion_tokens")) {
                last_completion_tokens_ = usage["completion_tokens"].get<size_t>();
            }
        }
        
        return common::Result<std::string>::Ok(content);
    } catch (const std::exception& e) {
        std::string error_msg = "Failed to parse Wenxin API response: ";
        error_msg += e.what();
        return common::Result<std::string>::Error(error_msg);
    }
}

void WenxinModel::HandleStreamChunk(const std::string& chunk, 
                                 StreamCallback callback, 
                                 bool& is_done) {
    if (chunk.empty()) {
        return;
    }
    
    try {
        auto json_response = json::parse(chunk);
        
        if (json_response.contains("error_code")) {
            spdlog::error("Error in stream response: {}", chunk);
            return;
        }
        
        if (json_response.contains("result")) {
            std::string content = json_response["result"].get<std::string>();
            callback(content, false);
        }
        
        if (json_response.contains("is_end") && json_response["is_end"].get<bool>()) {
            is_done = true;
            callback("", true);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error parsing stream chunk: {}", e.what());
    }
}

void WenxinModel::CalculateAndStoreTokenCounts(const std::vector<models::Message>& messages, 
                                           const std::string& response) {
    size_t prompt_tokens = 0;
    for (const auto& message : messages) {
        prompt_tokens += message.content.size() / 4;
    }
    
    size_t completion_tokens = response.size() / 4;
    
    last_prompt_tokens_ = prompt_tokens;
    last_completion_tokens_ = completion_tokens;
}

} // namespace ai_backend::services::ai