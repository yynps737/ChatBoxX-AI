#include "services/ai/models/tongyi_model.h"
#include "core/config/config_manager.h"
#include "core/http/request.h"
#include "core/utils/string_utils.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai_backend::services::ai {

using json = nlohmann::json;
using namespace core::async;

TongyiModel::TongyiModel() 
    : last_prompt_tokens_(0), 
      last_completion_tokens_(0),
      is_healthy_(true) {
    
    auto& config = core::config::ConfigManager::GetInstance();
    api_key_ = config.GetString("ai.tongyi.api_key", "");
    api_base_url_ = config.GetString("ai.tongyi.base_url", "https://api.tongyi.aliyun.com/v1");
    
    if (api_key_.empty()) {
        spdlog::error("Tongyi API key not configured");
        is_healthy_ = false;
    }
}

TongyiModel::~TongyiModel() = default;

std::string TongyiModel::GetModelId() const {
    return MODEL_ID;
}

std::string TongyiModel::GetModelName() const {
    return MODEL_NAME;
}

std::string TongyiModel::GetModelProvider() const {
    return MODEL_PROVIDER;
}

std::vector<std::string> TongyiModel::GetCapabilities() const {
    return {
        "text_generation", 
        "reasoning", 
        "creative_writing",
        "summarization",
        "translation"
    };
}

bool TongyiModel::SupportsStreaming() const {
    return true;
}

Task<common::Result<std::string>> 
TongyiModel::GenerateResponse(const std::vector<models::Message>& messages, 
                           const ModelConfig& config) {
    try {
        auto request = BuildAPIRequest(messages, config, false);
        
        auto response = co_await request.SendAsync();
        
        if (response.status_code != 200) {
            std::string error_msg = "Tongyi API error: " + 
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
        std::string error_msg = "Exception in TongyiModel::GenerateResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        co_return common::Result<std::string>::Error(error_msg);
    }
}

Task<common::Result<void>> 
TongyiModel::GenerateStreamingResponse(const std::vector<models::Message>& messages,
                                    StreamCallback callback,
                                    const ModelConfig& config) {
    try {
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
            std::string error_msg = "Tongyi API streaming error: " + 
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
        std::string error_msg = "Exception in TongyiModel::GenerateStreamingResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        callback("", true);
        co_return common::Result<void>::Error(error_msg);
    }
}

size_t TongyiModel::GetLastPromptTokens() const {
    return last_prompt_tokens_;
}

size_t TongyiModel::GetLastCompletionTokens() const {
    return last_completion_tokens_;
}

size_t TongyiModel::GetLastTotalTokens() const {
    return last_prompt_tokens_ + last_completion_tokens_;
}

bool TongyiModel::IsHealthy() const {
    return is_healthy_;
}

void TongyiModel::Reset() {
    last_prompt_tokens_ = 0;
    last_completion_tokens_ = 0;
}

core::http::Request 
TongyiModel::BuildAPIRequest(const std::vector<models::Message>& messages,
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
    request_body["model"] = API_MODEL;
    request_body["stream"] = stream;
    
    request_body["temperature"] = config.temperature;
    request_body["max_tokens"] = config.max_tokens;
    request_body["top_p"] = config.top_p;
    
    if (!config.stop_sequences.empty()) {
        request_body["stop"] = config.stop_sequences;
    }
    
    for (const auto& [key, value] : config.additional_params) {
        request_body[key] = value;
    }
    
    core::http::Request request;
    request.url = api_base_url_ + "/chat/completions";
    request.method = "POST";
    request.headers = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer " + api_key_}
    };
    request.body = request_body.dump();
    
    return request;
}

common::Result<std::string> 
TongyiModel::ParseAPIResponse(const std::string& response) {
    try {
        auto json_response = json::parse(response);
        
        if (json_response.contains("error")) {
            std::string error_msg = "Tongyi API error: ";
            if (json_response["error"].contains("message")) {
                error_msg += json_response["error"]["message"].get<std::string>();
            } else {
                error_msg += "Unknown error";
            }
            return common::Result<std::string>::Error(error_msg);
        }
        
        if (!json_response.contains("choices") || 
            json_response["choices"].empty() ||
            !json_response["choices"][0].contains("message") ||
            !json_response["choices"][0]["message"].contains("content")) {
            return common::Result<std::string>::Error("Invalid response format from Tongyi API");
        }
        
        std::string content = json_response["choices"][0]["message"]["content"].get<std::string>();
        
        if (json_response.contains("usage")) {
            last_prompt_tokens_ = json_response["usage"]["prompt_tokens"].get<size_t>();
            last_completion_tokens_ = json_response["usage"]["completion_tokens"].get<size_t>();
        }
        
        return common::Result<std::string>::Ok(content);
    } catch (const std::exception& e) {
        std::string error_msg = "Failed to parse Tongyi API response: ";
        error_msg += e.what();
        return common::Result<std::string>::Error(error_msg);
    }
}

void TongyiModel::HandleStreamChunk(const std::string& chunk, 
                                 StreamCallback callback, 
                                 bool& is_done) {
    if (chunk.empty()) {
        return;
    }
    
    try {
        if (chunk.find("data: ") == 0) {
            std::string json_str = chunk.substr(6);
            
            if (json_str == "[DONE]") {
                is_done = true;
                callback("", true);
                return;
            }
            
            auto json_response = json::parse(json_str);
            
            if (json_response.contains("choices") && 
                !json_response["choices"].empty() &&
                json_response["choices"][0].contains("delta") &&
                json_response["choices"][0]["delta"].contains("content")) {
                
                std::string content_delta = json_response["choices"][0]["delta"]["content"].get<std::string>();
                callback(content_delta, false);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error parsing stream chunk: {}", e.what());
    }
}

void TongyiModel::CalculateAndStoreTokenCounts(const std::vector<models::Message>& messages, 
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