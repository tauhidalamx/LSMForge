#pragma once

#include "lsmforge/slice.h"
#include <cstdint>
#include <string>

namespace lsmforge {

using SequenceNumber = uint64_t;

// We reserve 8 bits for the type, leaving 56 bits for the sequence number.
static constexpr SequenceNumber kMaxSequenceNumber = ((0x1ULL << 56) - 1);

enum ValueType : uint8_t {
    kTypeDeletion = 0x0,
    kTypeValue    = 0x1
};

// Value types encoded in database files
static constexpr ValueType kTypeValueTypeForSeek = kTypeValue;

struct ParsedInternalKey {
    Slice user_key;
    SequenceNumber sequence{0};
    ValueType type{kTypeValue};

    ParsedInternalKey() = default;
    ParsedInternalKey(Slice u, SequenceNumber s, ValueType t)
        : user_key(u), sequence(s), type(t) {}
};

inline uint64_t PackSequenceAndType(SequenceNumber seq, ValueType t) {
    return (seq << 8) | static_cast<uint8_t>(t);
}

inline void UnpackSequenceAndType(uint64_t packed, SequenceNumber* seq, ValueType* t) {
    *seq = packed >> 8;
    *t = static_cast<ValueType>(packed & 0xff);
}

inline void AppendInternalKey(std::string* result, const ParsedInternalKey& key) {
    result->append(key.user_key.data(), key.user_key.size());
    uint64_t packed = PackSequenceAndType(key.sequence, key.type);
    for (int i = 0; i < 8; ++i) {
        result->push_back(static_cast<char>((packed >> (i * 8)) & 0xff));
    }
}

inline bool ParseInternalKey(const Slice& internal_key, ParsedInternalKey* result) {
    const size_t n = internal_key.size();
    if (n < 8) return false;
    uint64_t packed = 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(internal_key.data() + n - 8);
    for (int i = 0; i < 8; ++i) {
        packed |= (static_cast<uint64_t>(p[i]) << (i * 8));
    }
    result->user_key = Slice(internal_key.data(), n - 8);
    UnpackSequenceAndType(packed, &result->sequence, &result->type);
    return (result->type <= kTypeValue);
}

inline Slice ExtractUserKey(const Slice& internal_key) {
    assert(internal_key.size() >= 8);
    return Slice(internal_key.data(), internal_key.size() - 8);
}

} // namespace lsmforge
