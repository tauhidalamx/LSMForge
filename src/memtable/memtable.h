#pragma once

#include "lsmforge/comparator.h"
#include "lsmforge/iterator.h"
#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "lsmforge/types.h"
#include "memtable/skiplist.h"
#include "util/arena.h"
#include "util/coding.h"
#include <atomic>
#include <string>

namespace lsmforge {

// Helper class for lookups
class LookupKey {
public:
    // Initialize key for looking up "user_key" at sequence number "s"
    LookupKey(const Slice& user_key, SequenceNumber sequence);
    ~LookupKey();

    LookupKey(const LookupKey&) = delete;
    LookupKey& operator=(const LookupKey&) = delete;

    // Return a key suitable for lookup in a MemTable
    [[nodiscard]] Slice memtable_key() const {
        return Slice(start_, static_cast<size_t>(end_ - start_));
    }

    // Return an internal key (user_key + seq + type)
    [[nodiscard]] Slice internal_key() const {
        return Slice(kstart_, static_cast<size_t>(end_ - kstart_));
    }

    // Return the user key
    [[nodiscard]] Slice user_key() const {
        return Slice(kstart_, static_cast<size_t>(end_ - kstart_ - 8));
    }

private:
    const char* start_;
    const char* kstart_;
    const char* end_;
    char space_[200]; // Avoid dynamic allocation for typical short keys
};

class MemTable {
public:
    explicit MemTable(const InternalKeyComparator& comparator);

    MemTable(const MemTable&) = delete;
    MemTable& operator=(const MemTable&) = delete;

    // Increase reference count
    void Ref() { ++refs_; }

    // Drop reference count; delete this MemTable when count reaches zero
    void Unref() {
        --refs_;
        assert(refs_ >= 0);
        if (refs_ <= 0) {
            delete this;
        }
    }

    // Returns an estimate of the number of bytes of data in use by this MemTable.
    [[nodiscard]] size_t ApproximateMemoryUsage() const {
        return arena_.MemoryUsage();
    }

    // Return an iterator over the contents of the memtable.
    // The returned iterator is valid until the memtable is destroyed.
    [[nodiscard]] Iterator* NewIterator();

    // Add an entry into memtable that maps key to value at the
    // specified sequence number and with the specified type.
    void Add(SequenceNumber seq, ValueType type, const Slice& key, const Slice& value);

    // If memtable contains a value for key, store it in *value and return true.
    // If memtable contains a deletion for key, store a NotFound() error
    // in *status and return true.
    // Else, return false.
    bool Get(const LookupKey& key, std::string* value, Status* status);

private:
    ~MemTable(); // Private destructor: use Unref()

    struct KeyComparator {
        const InternalKeyComparator comparator;
        explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
        int operator()(const char* a, const char* b) const;
    };

    friend class MemTableIterator;

    using Table = SkipList<const char*, KeyComparator>;

    KeyComparator comparator_;
    int refs_{0};
    Arena arena_;
    Table table_;
};

} // namespace lsmforge
