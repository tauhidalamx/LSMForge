#pragma once

#include "lsmforge/slice.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lsmforge {

class BloomFilterPolicy {
public:
    // bits_per_key: typically 10 gives ~1% false positive rate
    explicit BloomFilterPolicy(int bits_per_key);
    ~BloomFilterPolicy() = default;

    [[nodiscard]] const char* Name() const noexcept { return "lsmforge.BuiltinBloomFilter"; }

    // Generates a bloom filter bit array for the given keys and appends it to *dst.
    void CreateFilter(const Slice* keys, int n, std::string* dst) const;

    // Checks if the key may be present in the filter string.
    // Must NEVER return false if key was inserted in CreateFilter.
    [[nodiscard]] bool KeyMayMatch(const Slice& key, const Slice& filter) const;

    // Static hash function (MurmurHash3 32-bit)
    static uint32_t Hash(const char* data, size_t n, uint32_t seed = 0xbc9f1d34);

private:
    size_t bits_per_key_;
    size_t k_; // Number of hash functions
};

} // namespace lsmforge
