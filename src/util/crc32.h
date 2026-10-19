#pragma once

#include <cstddef>
#include <cstdint>

namespace lsmforge {
namespace crc32 {

// Return the crc32c of data[0,n-1]
uint32_t Value(const char* data, size_t n);

// Extend an existing crc32c value with data[0,n-1]
uint32_t Extend(uint32_t init_crc, const char* data, size_t n);

// Mask a crc value to avoid checksumming a checksum stored in memory/disk
static const uint32_t kMaskDelta = 0xa282ead8ul;

inline uint32_t Mask(uint32_t crc) {
    return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

inline uint32_t Unmask(uint32_t masked_crc) {
    uint32_t rot = masked_crc - kMaskDelta;
    return ((rot >> 17) | (rot << 15));
}

} // namespace crc32
} // namespace lsmforge
