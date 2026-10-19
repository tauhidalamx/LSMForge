#pragma once

#include "lsmforge/comparator.h"
#include "lsmforge/iterator.h"
#include "table/format.h"
#include <cstddef>
#include <cstdint>

namespace lsmforge {

class Block {
public:
    // Initialize the block with the specified contents.
    explicit Block(BlockContents contents);

    ~Block() = default;

    Block(const Block&) = delete;
    Block& operator=(const Block&) = delete;

    [[nodiscard]] size_t size() const noexcept { return size_; }

    [[nodiscard]] Iterator* NewIterator(const Comparator* comparator);

private:
    class Iter;

    BlockContents contents_;
    const char* data_;
    size_t size_;
    uint32_t restart_offset_; // Offset in data_ of restart array
};

} // namespace lsmforge
