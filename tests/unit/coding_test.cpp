#include "util/coding.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(CodingTest, Fixed32) {
    char buf[4];
    EncodeFixed32(buf, 0x12345678);
    EXPECT_EQ(DecodeFixed32(buf), 0x12345678);

    std::string s;
    PutFixed32(&s, 0xaabbccdd);
    EXPECT_EQ(DecodeFixed32(s.data()), 0xaabbccdd);
}

TEST(CodingTest, Fixed64) {
    char buf[8];
    EncodeFixed64(buf, 0x1122334455667788ULL);
    EXPECT_EQ(DecodeFixed64(buf), 0x1122334455667788ULL);

    std::string s;
    PutFixed64(&s, 0xfedcba9876543210ULL);
    EXPECT_EQ(DecodeFixed64(s.data()), 0xfedcba9876543210ULL);
}

TEST(CodingTest, Varint32) {
    std::vector<uint32_t> values = {0, 1, 127, 128, 16383, 16384, 0x7fffffff, 0xffffffff};
    std::string s;
    for (uint32_t v : values) {
        PutVarint32(&s, v);
    }

    Slice input(s);
    for (uint32_t expected : values) {
        uint32_t actual;
        EXPECT_TRUE(GetVarint32(&input, &actual));
        EXPECT_EQ(actual, expected);
    }
    EXPECT_TRUE(input.empty());
}

TEST(CodingTest, Varint64) {
    std::vector<uint64_t> values = {0, 1, 127, 128, 16383, 16384, (1ULL << 32) - 1,
                                    (1ULL << 32), (1ULL << 63), ~0ULL};
    std::string s;
    for (uint64_t v : values) {
        PutVarint64(&s, v);
    }

    Slice input(s);
    for (uint64_t expected : values) {
        uint64_t actual;
        EXPECT_TRUE(GetVarint64(&input, &actual));
        EXPECT_EQ(actual, expected);
    }
    EXPECT_TRUE(input.empty());
}

TEST(CodingTest, LengthPrefixedSlice) {
    std::string s;
    PutLengthPrefixedSlice(&s, "hello");
    PutLengthPrefixedSlice(&s, "world");

    Slice input(s);
    Slice result;
    EXPECT_TRUE(GetLengthPrefixedSlice(&input, &result));
    EXPECT_EQ(result, "hello");
    EXPECT_TRUE(GetLengthPrefixedSlice(&input, &result));
    EXPECT_EQ(result, "world");
    EXPECT_TRUE(input.empty());
}

} // namespace lsmforge
