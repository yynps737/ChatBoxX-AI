#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include <memory>

namespace ai_backend::core::memory {

class MemoryPool {
public:
    MemoryPool(size_t block_size, size_t initial_blocks = 32);
    ~MemoryPool();
    
    void* Allocate(size_t size);
    void Deallocate(void* ptr);
    
    size_t GetBlockSize() const;
    size_t GetUsedBlocks() const;
    size_t GetFreeBlocks() const;
    size_t GetTotalBlocks() const;
    
private:
    void AllocateNewBlock();
    
private:
    size_t block_size_;
    std::mutex mutex_;
    std::vector<char*> memory_blocks_;
    std::vector<void*> free_blocks_;
    std::atomic<size_t> used_blocks_{0};
};

template <typename T>
class PoolAllocator {
public:
    using value_type = T;
    
    PoolAllocator(MemoryPool& pool) : pool_(pool) {}
    
    template <typename U>
    PoolAllocator(const PoolAllocator<U>& other) : pool_(other.pool_) {}
    
    T* allocate(std::size_t n) {
        if (n * sizeof(T) > pool_.GetBlockSize()) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(pool_.Allocate(n * sizeof(T)));
    }
    
    void deallocate(T* p, std::size_t n) {
        pool_.Deallocate(p);
    }
    
    template <typename U>
    bool operator==(const PoolAllocator<U>& other) const {
        return &pool_ == &other.pool_;
    }
    
    template <typename U>
    bool operator!=(const PoolAllocator<U>& other) const {
        return !(*this == other);
    }
    
private:
    MemoryPool& pool_;
    
    template <typename U>
    friend class PoolAllocator;
};

} // namespace ai_backend::core::memory