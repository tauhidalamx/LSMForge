#include "util/crc32.h"

namespace lsmforge {
namespace crc32 {

namespace {

// CRC-32C (Castagnoli) polynomial in reversed representation: 0x82F63B78
constexpr uint32_t kPoly = 0x82F63B78ul;

class CrcTable {
public:
    uint32_t table[256];
    constexpr CrcTable() : table() {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) {
                c = (c & 1) ? (kPoly ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
    }
};

constexpr CrcTable kCrcTable;

} // namespace

uint32_t Extend(uint32_t init_crc, const char* data, size_t n) {
    uint32_t c = init_crc ^ 0xfffffffful;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    while (n--) {
        c = kCrcTable.table[(c ^ *p++) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xfffffffful;
}

uint32_t Value(const char* data, size_t n) {
    return Extend(0, data, n);
}

} // namespace crc32
} // namespace lsmforge
