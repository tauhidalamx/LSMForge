#pragma once

#include <cstdint>

namespace lsmforge {
namespace wal {

enum RecordType : uint8_t {
    // Zero is reserved for preallocated files or trailing block padding
    kZeroType   = 0,
    kFullType   = 1,
    kFirstType  = 2,
    kMiddleType = 3,
    kLastType   = 4,
    kMaxRecordType = kLastType
};

static constexpr size_t kBlockSize = 32768; // 32KB

// Header is checksum (4 bytes), length (2 bytes), type (1 byte)
static constexpr size_t kHeaderSize = 4 + 2 + 1;

} // namespace wal
} // namespace lsmforge
