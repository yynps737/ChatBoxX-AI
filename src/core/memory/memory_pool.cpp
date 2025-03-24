#include "core/memory/memory_pool.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::memory {

MemoryPool::MemoryPool(size_t block_size, size_t initial_blocks)
    : block_size_(block_size) {
    for (size_t i = 0; i < initial_blocks; ++i) {
        AllocateNewBlock();
    }
}

MemoryPool::~MemoryPool() {
    for (auto* block : memory_blocks_) {
        delete[] block;
    }
}

void* MemoryPool::Allocate(size_t size) {
    if (size > block_size_) {
        throw std::bad_alloc();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (free_blocks_.empty()) {
        AllocateNewBlock();
    }
    
    void* ptr = free_blocks_.back();
    free_blocks_.pop_back();
    used_blocks_++;
    
    return ptr;
}

void MemoryPool::Deallocate(void* ptr) {
    if (!ptr) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    free_blocks_.push_back(ptr);
    used_blocks_--;
}

size_t MemoryPool::GetBlockSize() const {
    return block_size_;
}

size_t MemoryPool::GetUsedBlocks() const {
    return used_blocks_;
}

size_t MemoryPool::GetFreeBlocks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return free_blocks_.size();
}

size_t MemoryPool::GetTotalBlocks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return memory_blocks_.size() * (block_size_ / sizeof(void*));
}

void MemoryPool::AllocateNewBlock() {
    char* block = new char[block_size_ * 32];
    memory_blocks_.push_back(block);
    
    for (size_t i = 0; i < 32; ++i) {
        free_blocks_.push_back(block + i * block_size_);
    }
}

} // namespace ai_backend::core::memory