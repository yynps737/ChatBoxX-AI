#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/async/task.h"
#include "services/ai/model_interface.h"
#include "models/message.h"

namespace ai_backend::services::ai::streaming {

struct StreamChunk {
    std::string content;
    bool is_done;
    bool is_error;
    std::string error_message;
};

using StreamCallback = std::function<void(const StreamChunk&)>;

class StreamProcessor {
public:
    explicit StreamProcessor(size_t buffer_size = 50);
    ~StreamProcessor();
    
    core::async::Task<void> ProcessStream(
        std::shared_ptr<ModelInterface> model,
        const std::vector<models::Message>& messages,
        StreamCallback callback,
        const ModelInterface::ModelConfig& config = {}
    );
    
    void Stop();

private:
    void HandleStreamChunk(
        const std::string& content,
        bool is_done,
        StreamCallback callback
    );

private:
    size_t buffer_size_;
    std::string buffer_;
    std::atomic<bool> should_stop_;
};

} // namespace ai_backend::services::ai::streaming