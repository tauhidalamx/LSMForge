#pragma once

#include "lsmforge/options.h"
#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "util/env.h"
#include <cstdint>
#include <string>

namespace lsmforge {

// BlockHandle is a pointer to the extent of a file that stores a data
// block or a meta block.
class BlockHandle {
public:
    enum { kMaxEncodedLength = 10 + 10 };

    BlockHandle() : offset_(~static_cast<uint64_t>(0)), size_(~static_cast<uint64_t>(0)) {}

    [[nodiscard]] uint64_t offset() const noexcept { return offset_; }
    void set_offset(uint64_t offset) noexcept { offset_ = offset; }

    [[nodiscard]] uint64_t size() const noexcept { return size_; }
    void set_size(uint64_t size) noexcept { size_ = size; }

    void EncodeTo(std::string* dst) const;
    Status DecodeFrom(Slice* input);

private:
    uint64_t offset_;
    uint64_t size_;
};

// Footer encapsulates the fixed information stored at the tail end of every SSTable.
class Footer {
public:
    enum {
        kEncodedLength = 2 * BlockHandle::kMaxEncodedLength + 8, // 48 bytes
    };

    // "LSMForge" magic number
    static constexpr uint64_t kTableMagicNumber = 0x4c534d466f726765ULL;

    Footer() = default;

    [[nodiscard]] const BlockHandle& metaindex_handle() const noexcept { return metaindex_handle_; }
    void set_metaindex_handle(const BlockHandle& h) noexcept { metaindex_handle_ = h; }

    [[nodiscard]] const BlockHandle& index_handle() const noexcept { return index_handle_; }
    void set_index_handle(const BlockHandle& h) noexcept { index_handle_ = h; }

    void EncodeTo(std::string* dst) const;
    Status DecodeFrom(Slice* input);

private:
    BlockHandle metaindex_handle_;
    BlockHandle index_handle_;
};

// 1-byte compression type + 4-byte CRC32
static constexpr size_t kBlockTrailerSize = 1 + 4;

struct BlockContents {
    Slice data;          // Actual block contents
    bool cachable{false}; // True if data can be cached
    bool heap_allocated{false}; // True if data was allocated via new[]

    BlockContents() = default;
    ~BlockContents() {
        if (heap_allocated) {
            delete[] data.data();
        }
    }

    BlockContents(const BlockContents&) = delete;
    BlockContents& operator=(const BlockContents&) = delete;

    BlockContents(BlockContents&& o) noexcept
        : data(o.data), cachable(o.cachable), heap_allocated(o.heap_allocated) {
        o.heap_allocated = false;
    }

    BlockContents& operator=(BlockContents&& o) noexcept {
        if (this != &o) {
            if (heap_allocated) {
                delete[] data.data();
            }
            data = o.data;
            cachable = o.cachable;
            heap_allocated = o.heap_allocated;
            o.heap_allocated = false;
        }
        return *this;
    }
};

// Read the block identified by "handle" from "file". On failure
// returns non-OK. On success fills *result and returns OK.
Status ReadBlock(RandomAccessFile* file, const ReadOptions& options,
                 const BlockHandle& handle, BlockContents* result);

} // namespace lsmforge
