#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "util/env.h"
#include "wal/wal_format.h"
#include <cstdint>

namespace lsmforge {
namespace wal {

class WalWriter {
public:
    // Create a writer that will append data to "*dest".
    // "*dest" must remain valid for the lifetime of this WalWriter.
    explicit WalWriter(WritableFile* dest, uint64_t dest_length = 0);
    ~WalWriter() = default;

    WalWriter(const WalWriter&) = delete;
    WalWriter& operator=(const WalWriter&) = delete;

    Status AddRecord(const Slice& slice);

    [[nodiscard]] WritableFile* file() const noexcept { return dest_; }

private:
    Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);

    WritableFile* dest_;
    int block_offset_; // Current offset within 32KB block
};

} // namespace wal
} // namespace lsmforge
