#include "wal/wal_reader.h"
#include "util/coding.h"
#include "util/crc32.h"
#include <cassert>

namespace lsmforge {
namespace wal {

WalReader::WalReader(SequentialFile* file, Reporter* reporter, bool checksum,
                     uint64_t initial_offset)
    : file_(file),
      reporter_(reporter),
      checksum_(checksum),
      backing_store_(new char[kBlockSize]),
      buffer_(),
      eof_(false),
      last_record_offset_(0),
      end_of_buffer_offset_(0),
      initial_offset_(initial_offset) {}

void WalReader::ReportCorruption(uint64_t bytes, const char* reason) {
    ReportDrop(bytes, Status::Corruption(reason));
}

void WalReader::ReportDrop(uint64_t bytes, const Status& reason) {
    if (reporter_ != nullptr &&
        end_of_buffer_offset_ - buffer_.size() - bytes >= initial_offset_) {
        reporter_->Corruption(static_cast<size_t>(bytes), reason);
    }
}

bool WalReader::ReadRecord(Slice* record, std::string* scratch) {
    if (last_record_offset_ < initial_offset_) {
        // Skip till initial offset if needed
    }

    scratch->clear();
    record->clear();
    bool in_fragmented_record = false;
    uint64_t prospective_record_offset = 0;

    Slice fragment;
    while (true) {
        const uint64_t physical_record_offset = end_of_buffer_offset_ - buffer_.size();
        const unsigned int record_type = ReadPhysicalRecord(&fragment);

        switch (record_type) {
            case kFullType:
                if (in_fragmented_record) {
                    ReportCorruption(scratch->size(), "partial record without end");
                }
                prospective_record_offset = physical_record_offset;
                scratch->clear();
                *record = fragment;
                last_record_offset_ = prospective_record_offset;
                return true;

            case kFirstType:
                if (in_fragmented_record) {
                    ReportCorruption(scratch->size(), "partial record without end");
                }
                prospective_record_offset = physical_record_offset;
                scratch->assign(fragment.data(), fragment.size());
                in_fragmented_record = true;
                break;

            case kMiddleType:
                if (!in_fragmented_record) {
                    ReportCorruption(fragment.size(), "missing start of fragmented record");
                } else {
                    scratch->append(fragment.data(), fragment.size());
                }
                break;

            case kLastType:
                if (!in_fragmented_record) {
                    ReportCorruption(fragment.size(), "missing start of fragmented record");
                } else {
                    scratch->append(fragment.data(), fragment.size());
                    *record = Slice(*scratch);
                    last_record_offset_ = prospective_record_offset;
                    return true;
                }
                break;

            case kEof:
                if (in_fragmented_record) {
                    // This can happen if the database crashed while writing a record
                    ReportCorruption(scratch->size(), "partial record at end of file");
                }
                return false;

            case kBadRecord:
                if (in_fragmented_record) {
                    ReportCorruption(scratch->size(), "error in middle of record");
                    in_fragmented_record = false;
                    scratch->clear();
                }
                break;

            case kBadRecordChecksum:
                if (in_fragmented_record) {
                    ReportCorruption(scratch->size(), "checksum error in middle of record");
                    in_fragmented_record = false;
                    scratch->clear();
                }
                break;

            default:
                ReportCorruption(fragment.size(), "unknown record type");
                in_fragmented_record = false;
                scratch->clear();
                break;
        }
    }
    return false;
}

unsigned int WalReader::ReadPhysicalRecord(Slice* result) {
    while (true) {
        if (buffer_.size() < kHeaderSize) {
            if (!eof_) {
                // Read a whole block
                buffer_.clear();
                Status status = file_->Read(kBlockSize, &buffer_, backing_store_);
                end_of_buffer_offset_ += buffer_.size();
                if (!status.ok()) {
                    buffer_.clear();
                    ReportDrop(kBlockSize, status);
                    eof_ = true;
                    return kEof;
                } else if (buffer_.size() < kBlockSize) {
                    eof_ = true;
                }
                continue;
            } else {
                // Note that if buffer_ is non-empty, we have a truncated header at the end of the file
                buffer_.clear();
                return kEof;
            }
        }

        // Parse the header
        const char* header = buffer_.data();
        const uint32_t a = static_cast<uint32_t>(static_cast<uint8_t>(header[4]));
        const uint32_t b = static_cast<uint32_t>(static_cast<uint8_t>(header[5]));
        const unsigned int type = static_cast<unsigned int>(header[6]);
        const uint32_t length = a | (b << 8);

        if (kHeaderSize + length > buffer_.size()) {
            size_t drop_size = buffer_.size();
            buffer_.clear();
            if (!eof_) {
                ReportCorruption(drop_size, "bad record length");
                return kBadRecord;
            }
            // If EOF, this was an incomplete trailing record
            return kEof;
        }

        if (type == kZeroType && length == 0) {
            // Skip zero bytes (typically padding at the end of a block)
            buffer_.clear();
            return kBadRecord;
        }

        // Check checksum
        if (checksum_) {
            uint32_t expected_crc = crc32::Unmask(DecodeFixed32(header));
            uint32_t actual_crc = crc32::Extend(0, header + 6, 1 + length);
            if (actual_crc != expected_crc) {
                // Drop the rest of the buffer
                size_t drop_size = buffer_.size();
                buffer_.clear();
                ReportCorruption(drop_size, "checksum mismatch");
                return kBadRecordChecksum;
            }
        }

        buffer_.remove_prefix(kHeaderSize + length);
        *result = Slice(header + kHeaderSize, length);
        return type;
    }
}

} // namespace wal
} // namespace lsmforge
