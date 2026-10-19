#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lsmforge {

class Arena {
public:
    Arena();
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    // Return a pointer to a newly allocated memory buffer of "bytes" bytes.
    char* Allocate(size_t bytes);

    // Allocate memory with standard pointer alignment (typically 8 bytes).
    char* AllocateAligned(size_t bytes);

    // Returns an estimate of the total memory usage of data allocated
    // by the arena in bytes.
    [[nodiscard]] size_t MemoryUsage() const noexcept {
        return memory_usage_.load(std::memory_order_relaxed);
    }

private:
    char* AllocateFallback(size_t bytes);
    char* AllocateNewBlock(size_t block_bytes);

    // Allocation state
    char* alloc_ptr_{nullptr};
    size_t alloc_bytes_remaining_{0};

    // Array of allocated memory blocks
    std::vector<char*> blocks_;

    // Total memory allocated
    std::atomic<size_t> memory_usage_{0};
};

} // namespace lsmforge
