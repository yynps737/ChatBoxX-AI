#include "services/ai/streaming/stream_processor.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace ai_backend::services::ai::streaming {

using json = nlohmann::json;
using namespace core::async;

StreamProcessor::StreamProcessor(size_t buffer_size)
    : buffer_size_(buffer_size),
      should_stop_(false) {
}

StreamProcessor::~StreamProcessor() {
    Stop();
}

Task<void> StreamProcessor::ProcessStream(
    std::shared_ptr<ModelInterface> model,
    const std::vector<models::Message>& messages,
    StreamCallback callback,
    const ModelInterface::ModelConfig& config) {
    
    should_stop_ = false;
    buffer_.clear();
    
    auto result = co_await model->GenerateStreamingResponse(
        messages,
        [this, callback](const std::string& content, bool is_done) {
            this->HandleStreamChunk(content, is_done, callback);
        },
        config
    );
    
    if (result.IsError()) {
        spdlog::error("Error in stream processing: {}", result.GetError());
        callback(StreamChunk{.content = "", .is_done = true, .is_error = true, .error_message = result.GetError()});
    }
    
    co_return;
}

void StreamProcessor::Stop() {
    should_stop_ = true;
}

void StreamProcessor::HandleStreamChunk(
    const std::string& content,
    bool is_done,
    StreamCallback callback) {
    
    if (should_stop_) {
        callback(StreamChunk{.content = "", .is_done = true, .is_error = false});
        return;
    }
    
    if (is_done) {
        // 发送剩余缓冲区内容
        if (!buffer_.empty()) {
            callback(StreamChunk{.content = buffer_, .is_done = false, .is_error = false});
            buffer_.clear();
        }
        
        // 发送完成标记
        callback(StreamChunk{.content = "", .is_done = true, .is_error = false});
        return;
    }
    
    // 添加到缓冲区
    buffer_ += content;
    
    // 如果缓冲区达到指定大小，发送并清空
    if (buffer_.size() >= buffer_size_) {
        callback(StreamChunk{.content = buffer_, .is_done = false, .is_error = false});
        buffer_.clear();
    }
}

} // namespace ai_backend::services::ai::streaming