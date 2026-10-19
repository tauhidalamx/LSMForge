#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "util/env.h"
#include "wal/wal_format.h"
#include <cstdint>
#include <string>

namespace lsmforge {
namespace wal {

class WalReader {
public:
    // Interface for reporting errors found during log reading.
    class Reporter {
    public:
        virtual ~Reporter() = default;
        virtual void Corruption(size_t bytes, const Status& status) = 0;
    };

    // Create a reader that will return log records from "*file".
    // "*file" must remain valid for the lifetime of this WalReader.
    WalReader(SequentialFile* file, Reporter* reporter, bool checksum,
              uint64_t initial_offset = 0);
    ~WalReader() = default;

    WalReader(const WalReader&) = delete;
    WalReader& operator=(const WalReader&) = delete;

    // Read the next record into *record. Returns true if read
    // successfully, false if we hit end of file or unrecoverable read error.
    // The contents of the record are only valid until the next call to ReadRecord.
    bool ReadRecord(Slice* record, std::string* scratch);

    // Returns the physical offset of the last record returned by ReadRecord.
    [[nodiscard]] uint64_t LastRecordOffset() const noexcept { return last_record_offset_; }

private:
    // Extend record by reading physical records
    unsigned int ReadPhysicalRecord(Slice* result);

    // Reports dropped bytes to the reporter.
    void ReportCorruption(uint64_t bytes, const char* reason);
    void ReportDrop(uint64_t bytes, const Status& reason);

    SequentialFile* const file_;
    Reporter* const reporter_;
    const bool checksum_;
    char* const backing_store_;
    Slice buffer_;
    bool eof_; // Last Read() returned a short result

    // Offset of the last record returned by ReadRecord.
    uint64_t last_record_offset_{0};
    // Offset of the end of the last record read by ReadPhysicalRecord.
    uint64_t end_of_buffer_offset_{0};

    // The physical offset from which we started reading.
    uint64_t const initial_offset_;

    // Status enum for ReadPhysicalRecord
    enum {
        kEof = kMaxRecordType + 1,
        kBadRecord = kMaxRecordType + 2,
        kBadRecordLen = kMaxRecordType + 3,
        kBadRecordChecksum = kMaxRecordType + 4
    };
};

} // namespace wal
} // namespace lsmforge
