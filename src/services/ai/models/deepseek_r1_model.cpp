#include "services/ai/models/deepseek_r1_model.h"
#include "core/config/config_manager.h"
#include "core/http/request.h"
#include "core/http/response.h"
#include "core/utils/string_utils.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace ai_backend::services::ai {

using json = nlohmann::json;
using namespace core::async;

DeepseekR1Model::DeepseekR1Model() 
    : last_prompt_tokens_(0), 
      last_completion_tokens_(0),
      is_healthy_(true) {
    // 从配置加载API密钥和URL
    auto& config = core::config::ConfigManager::GetInstance();
    api_key_ = config.GetString("ai.deepseek.api_key", "");
    api_base_url_ = config.GetString("ai.deepseek.base_url", "https://api.deepseek.com/v1");
    
    // 验证配置
    if (api_key_.empty()) {
        spdlog::error("DeepSeek API key not configured");
        is_healthy_ = false;
    }
}

DeepseekR1Model::~DeepseekR1Model() = default;

std::string DeepseekR1Model::GetModelId() const {
    return MODEL_ID;
}

std::string DeepseekR1Model::GetModelName() const {
    return MODEL_NAME;
}

std::string DeepseekR1Model::GetModelProvider() const {
    return MODEL_PROVIDER;
}

std::vector<std::string> DeepseekR1Model::GetCapabilities() const {
    return {
        "code_generation", 
        "code_completion", 
        "code_explanation",
        "text_generation"
    };
}

bool DeepseekR1Model::SupportsStreaming() const {
    return true;
}

Task<common::Result<std::string>> 
DeepseekR1Model::GenerateResponse(const std::vector<models::Message>& messages, 
                                const ModelConfig& config) {
    try {
        // 构建API请求
        auto request = BuildAPIRequest(messages, config, false);
        
        // 发送请求
        auto response = co_await request.SendAsync();
        
        if (response.status_code != 200) {
            std::string error_msg = "DeepSeek API error: " + 
                                  std::to_string(response.status_code) + " " + 
                                  response.body;
            spdlog::error(error_msg);
            co_return common::Result<std::string>::Error(error_msg);
        }
        
        // 解析响应
        auto result = ParseAPIResponse(response.body);
        
        // 计算token使用情况
        if (result.IsOk()) {
            CalculateAndStoreTokenCounts(messages, result.GetValue());
        }
        
        co_return result;
    } catch (const std::exception& e) {
        std::string error_msg = "Exception in DeepseekR1Model::GenerateResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        co_return common::Result<std::string>::Error(error_msg);
    }
}

Task<common::Result<void>> 
DeepseekR1Model::GenerateStreamingResponse(const std::vector<models::Message>& messages,
                                         StreamCallback callback,
                                         const ModelConfig& config) {
    try {
        // 构建API请求
        auto request = BuildAPIRequest(messages, config, true);
        
        // 发送流式请求
        bool is_done = false;
        size_t tokens = 0;
        
        auto stream_handler = [this, &callback, &is_done, &tokens]
                              (const std::string& chunk) {
            this->HandleStreamChunk(chunk, callback, is_done);
            tokens++;
        };
        
        auto response = co_await request.SendStreamAsync(stream_handler);
        
        if (response.status_code != 200) {
            std::string error_msg = "DeepSeek API streaming error: " + 
                                  std::to_string(response.status_code);
            spdlog::error(error_msg);
            callback("", true); // 标记完成
            co_return common::Result<void>::Error(error_msg);
        }
        
        // 确保完成回调被调用
        if (!is_done) {
            callback("", true);
        }
        
        // 设置完成tokens
        last_completion_tokens_ = tokens;
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        std::string error_msg = "Exception in DeepseekR1Model::GenerateStreamingResponse: ";
        error_msg += e.what();
        spdlog::error(error_msg);
        callback("", true); // 标记完成
        co_return common::Result<void>::Error(error_msg);
    }
}

size_t DeepseekR1Model::GetLastPromptTokens() const {
    return last_prompt_tokens_;
}

size_t DeepseekR1Model::GetLastCompletionTokens() const {
    return last_completion_tokens_;
}

size_t DeepseekR1Model::GetLastTotalTokens() const {
    return last_prompt_tokens_ + last_completion_tokens_;
}

bool DeepseekR1Model::IsHealthy() const {
    return is_healthy_;
}

void DeepseekR1Model::Reset() {
    last_prompt_tokens_ = 0;
    last_completion_tokens_ = 0;
}

core::http::Request 
DeepseekR1Model::BuildAPIRequest(const std::vector<models::Message>& messages,
                               const ModelConfig& config,
                               bool stream) const {
    json request_body;
    
    // 转换消息格式
    json messages_json = json::array();
    for (const auto& message : messages) {
        json msg_json;
        msg_json["role"] = message.role;
        msg_json["content"] = message.content;
        messages_json.push_back(msg_json);
    }
    
    request_body["messages"] = messages_json;
    request_body["model"] = "deepseek-coder-v1";
    request_body["stream"] = stream;
    
    // 添加配置参数
    request_body["temperature"] = config.temperature;
    request_body["max_tokens"] = config.max_tokens;
    request_body["top_p"] = config.top_p;
    
    if (!config.stop_sequences.empty()) {
        request_body["stop"] = config.stop_sequences;
    }
    
    // 添加其他参数
    for (const auto& [key, value] : config.additional_params) {
        request_body[key] = value;
    }
    
    // 构建HTTP请求
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
DeepseekR1Model::ParseAPIResponse(const std::string& response) {
    try {
        auto json_response = json::parse(response);
        
        if (json_response.contains("error")) {
            std::string error_msg = "DeepSeek API error: ";
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
            return common::Result<std::string>::Error("Invalid response format from DeepSeek API");
        }
        
        std::string content = json_response["choices"][0]["message"]["content"].get<std::string>();
        
        // 获取token计数
        if (json_response.contains("usage")) {
            last_prompt_tokens_ = json_response["usage"]["prompt_tokens"].get<size_t>();
            last_completion_tokens_ = json_response["usage"]["completion_tokens"].get<size_t>();
        }
        
        return common::Result<std::string>::Ok(content);
    } catch (const std::exception& e) {
        std::string error_msg = "Failed to parse DeepSeek API response: ";
        error_msg += e.what();
        return common::Result<std::string>::Error(error_msg);
    }
}

void DeepseekR1Model::HandleStreamChunk(const std::string& chunk, 
                                      StreamCallback callback, 
                                      bool& is_done) {
    if (chunk.empty()) {
        return;
    }
    
    try {
        // 处理SSE格式
        if (chunk.find("data: ") == 0) {
            std::string json_str = chunk.substr(6); // 跳过"data: "
            
            // 检查是否是[DONE]标记
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

void DeepseekR1Model::CalculateAndStoreTokenCounts(const std::vector<models::Message>& messages, 
                                                const std::string& response) {
    // 这里使用简单估算，实际项目中可能需要更复杂的tokenizer
    size_t prompt_tokens = 0;
    for (const auto& message : messages) {
        // 估算：平均每4个字符约等于1个token
        prompt_tokens += message.content.size() / 4;
    }
    
    size_t completion_tokens = response.size() / 4;
    
    last_prompt_tokens_ = prompt_tokens;
    last_completion_tokens_ = completion_tokens;
}

} // namespace ai_backend::services::ai