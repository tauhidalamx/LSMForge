#include "wal/wal_writer.h"
#include "util/coding.h"
#include "util/crc32.h"
#include <cassert>

namespace lsmforge {
namespace wal {

WalWriter::WalWriter(WritableFile* dest, uint64_t dest_length)
    : dest_(dest),
      block_offset_(static_cast<int>(dest_length % kBlockSize)) {}

Status WalWriter::AddRecord(const Slice& slice) {
    const char* ptr = slice.data();
    size_t left = slice.size();

    // Fragment the record if necessary and emit it
    Status s;
    bool begin = true;
    do {
        const int leftover = static_cast<int>(kBlockSize - block_offset_);
        assert(leftover >= 0);
        if (leftover < static_cast<int>(kHeaderSize)) {
            // Switch to a new block
            if (leftover > 0) {
                // Fill the rest of the block with zeroes
                static_assert(kHeaderSize <= 7, "");
                dest_->Append(Slice("\x00\x00\x00\x00\x00\x00\x00", leftover));
            }
            block_offset_ = 0;
        }

        // Invariant: we never leave less than kHeaderSize bytes in a block
        assert(kBlockSize - block_offset_ >= kHeaderSize);

        const size_t avail = kBlockSize - block_offset_ - kHeaderSize;
        const size_t fragment_length = (left < avail) ? left : avail;

        RecordType type;
        const bool end = (left == fragment_length);
        if (begin && end) {
            type = kFullType;
        } else if (begin) {
            type = kFirstType;
        } else if (end) {
            type = kLastType;
        } else {
            type = kMiddleType;
        }

        s = EmitPhysicalRecord(type, ptr, fragment_length);
        ptr += fragment_length;
        left -= fragment_length;
        begin = false;
    } while (s.ok() && left > 0);

    return s;
}

Status WalWriter::EmitPhysicalRecord(RecordType t, const char* ptr, size_t length) {
    assert(length <= 0xffff); // Must fit in 16-bit unsigned int
    assert(block_offset_ + kHeaderSize + length <= kBlockSize);

    // Format the header
    char buf[kHeaderSize];
    buf[4] = static_cast<char>(length & 0xff);
    buf[5] = static_cast<char>((length >> 8) & 0xff);
    buf[6] = static_cast<char>(t);

    // Compute the crc of the record type and the payload
    uint32_t crc = crc32::Extend(0, &buf[6], 1);
    crc = crc32::Extend(crc, ptr, length);
    crc = crc32::Mask(crc);
    EncodeFixed32(buf, crc);

    // Write the header and the payload
    Status s = dest_->Append(Slice(buf, kHeaderSize));
    if (s.ok()) {
        s = dest_->Append(Slice(ptr, length));
        if (s.ok()) {
            s = dest_->Flush();
        }
    }
    block_offset_ += static_cast<int>(kHeaderSize + length);
    return s;
}

} // namespace wal
} // namespace lsmforge
