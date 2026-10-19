#include "util/crc32.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(Crc32Test, BasicChecksum) {
    const char data[] = "123456789";
    uint32_t crc = crc32::Value(data, 9);
    // Standard CRC-32C for "123456789" is 0xe3069283
    EXPECT_EQ(crc, 0xe3069283ul);
}

TEST(Crc32Test, ExtendChecksum) {
    const char part1[] = "1234";
    const char part2[] = "56789";
    uint32_t crc1 = crc32::Value(part1, 4);
    uint32_t full_crc = crc32::Extend(crc1, part2, 5);
    EXPECT_EQ(full_crc, 0xe3069283ul);
}

TEST(Crc32Test, MaskAndUnmask) {
    uint32_t crc = 0x12345678;
    uint32_t masked = crc32::Mask(crc);
    EXPECT_NE(masked, crc);
    EXPECT_EQ(crc32::Unmask(masked), crc);
}

} // namespace lsmforge
