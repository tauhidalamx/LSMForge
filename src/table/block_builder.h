#pragma once

#include "lsmforge/options.h"
#include "lsmforge/slice.h"
#include <cstdint>
#include <string>
#include <vector>

namespace lsmforge {

class BlockBuilder {
public:
    explicit BlockBuilder(const Options* options);

    BlockBuilder(const BlockBuilder&) = delete;
    BlockBuilder& operator=(const BlockBuilder&) = delete;

    // Reset the contents as if the BlockBuilder was just constructed.
    void Reset();

    // REQUIRES: Finish() has not been called since the last call to Reset().
    // REQUIRES: key is larger than any previously added key
    void Add(const Slice& key, const Slice& value);

    // Finish building the block and return a slice that refers to the
    // block contents. The returned slice will remain valid until the
    // next call to Reset().
    Slice Finish();

    // Returns an estimate of the current size of the block being built.
    [[nodiscard]] size_t CurrentSizeEstimate() const;

    // Return true iff no entries have been added since the last Reset().
    [[nodiscard]] bool empty() const noexcept { return buffer_.empty(); }

private:
    const Options* options_;
    std::string buffer_;              // Destination buffer
    std::vector<uint32_t> restarts_;  // Restart points
    int counter_{0};                  // Number of entries emitted since restart
    bool finished_{false};            // Has Finish() been called?
    std::string last_key_;
};

} // namespace lsmforge
