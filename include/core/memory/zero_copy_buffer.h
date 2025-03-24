#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ai_backend::core::memory {

class ZeroCopyBuffer {
public:
    ZeroCopyBuffer(size_t initial_capacity = 4096);
    ~ZeroCopyBuffer();
    
    void Clear();
    void Append(const char* data, size_t size);
    void Append(const std::string& data);
    
    const char* Data() const;
    size_t Size() const;
    
    std::string ToString() const;
    std::vector<uint8_t> ToVector() const;
    
    void Reserve(size_t capacity);
    size_t Capacity() const;
    
private:
    char* buffer_;
    size_t size_;
    size_t capacity_;
};

} // namespace ai_backend::core::memory