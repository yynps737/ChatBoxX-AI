#include "core/memory/zero_copy_buffer.h"
#include <cstring>
#include <stdexcept>

namespace ai_backend::core::memory {

ZeroCopyBuffer::ZeroCopyBuffer(size_t initial_capacity)
    : size_(0), capacity_(initial_capacity) {
    buffer_ = new char[capacity_];
}

ZeroCopyBuffer::~ZeroCopyBuffer() {
    delete[] buffer_;
}

void ZeroCopyBuffer::Clear() {
    size_ = 0;
}

void ZeroCopyBuffer::Append(const char* data, size_t size) {
    if (size_ + size > capacity_) {
        Reserve(std::max(capacity_ * 2, size_ + size));
    }
    
    std::memcpy(buffer_ + size_, data, size);
    size_ += size;
}

void ZeroCopyBuffer::Append(const std::string& data) {
    Append(data.data(), data.size());
}

const char* ZeroCopyBuffer::Data() const {
    return buffer_;
}

size_t ZeroCopyBuffer::Size() const {
    return size_;
}

std::string ZeroCopyBuffer::ToString() const {
    return std::string(buffer_, size_);
}

std::vector<uint8_t> ZeroCopyBuffer::ToVector() const {
    return std::vector<uint8_t>(buffer_, buffer_ + size_);
}

void ZeroCopyBuffer::Reserve(size_t capacity) {
    if (capacity <= capacity_) {
        return;
    }
    
    char* new_buffer = new char[capacity];
    std::memcpy(new_buffer, buffer_, size_);
    
    delete[] buffer_;
    buffer_ = new_buffer;
    capacity_ = capacity;
}

size_t ZeroCopyBuffer::Capacity() const {
    return capacity_;
}

} // namespace ai_backend::core::memory