#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "lsmforge/types.h"
#include <string>

namespace lsmforge {

class MemTable;

class WriteBatch {
public:
    class Handler {
    public:
        virtual ~Handler() = default;
        virtual Status Put(const Slice& key, const Slice& value) = 0;
        virtual Status Delete(const Slice& key) = 0;
    };

    WriteBatch();
    ~WriteBatch() = default;

    WriteBatch(const WriteBatch&) = default;
    WriteBatch& operator=(const WriteBatch&) = default;
    WriteBatch(WriteBatch&&) noexcept = default;
    WriteBatch& operator=(WriteBatch&&) noexcept = default;

    // Store the mapping "key->value" in the database.
    void Put(const Slice& key, const Slice& value);

    // If the database contains a mapping for "key", erase it. Else do nothing.
    void Delete(const Slice& key);

    // Clear all updates buffered in this batch.
    void Clear();

    // The size of the serialized representation of this batch in bytes.
    [[nodiscard]] size_t ApproximateSize() const noexcept { return rep_.size(); }

    // Number of entries in this batch.
    [[nodiscard]] int Count() const noexcept;

    // Set the count of entries in the batch header.
    void SetCount(int n);

    // Support for iterating over the contents of a batch.
    Status Iterate(Handler* handler) const;

    // Retrieve raw representation for WAL writing.
    [[nodiscard]] const std::string& Data() const noexcept { return rep_; }

    // Set sequence number at header (first 8 bytes).
    void SetSequence(SequenceNumber seq);
    [[nodiscard]] SequenceNumber Sequence() const;

    // Append the contents of another batch to this batch.
    void Append(const WriteBatch& source);

private:
    friend class WriteBatchInternal;
    std::string rep_; // 8-byte sequence + 4-byte count + entries
};

} // namespace lsmforge
