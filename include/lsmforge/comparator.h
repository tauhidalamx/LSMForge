#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/types.h"
#include <string>

namespace lsmforge {

class Comparator {
public:
    virtual ~Comparator() = default;

    // Three-way comparison:
    //   < 0 iff "a" < "b"
    //  == 0 iff "a" == "b"
    //   > 0 iff "a" > "b"
    [[nodiscard]] virtual int Compare(const Slice& a, const Slice& b) const = 0;

    // The name of the comparator to detect database incompatibilities.
    [[nodiscard]] virtual const char* Name() const = 0;

    // Advanced: Shortest separator between *start and limit (like LevelDB/RocksDB)
    virtual void FindShortestSeparator(std::string* start, const Slice& limit) const = 0;
    virtual void FindShortSuccessor(std::string* key) const = 0;
};

// Returns a default comparator that uses lexicographic byte-wise ordering.
const Comparator* BytewiseComparator();

class InternalKeyComparator : public Comparator {
public:
    explicit InternalKeyComparator(const Comparator* user_comparator)
        : user_comparator_(user_comparator) {}

    [[nodiscard]] const char* Name() const override {
        return "lsmforge.InternalKeyComparator";
    }

    [[nodiscard]] int Compare(const Slice& a, const Slice& b) const override {
        int r = user_comparator_->Compare(ExtractUserKey(a), ExtractUserKey(b));
        if (r == 0) {
            ParsedInternalKey ikey_a;
            ParsedInternalKey ikey_b;
            if (ParseInternalKey(a, &ikey_a) && ParseInternalKey(b, &ikey_b)) {
                if (ikey_a.sequence > ikey_b.sequence) return -1;
                if (ikey_a.sequence < ikey_b.sequence) return 1;
                if (ikey_a.type > ikey_b.type) return -1;
                if (ikey_a.type < ikey_b.type) return 1;
            }
        }
        return r;
    }

    void FindShortestSeparator(std::string* start, const Slice& limit) const override {
        // Simple default: do not modify to preserve exact internal key format
    }

    void FindShortSuccessor(std::string* key) const override {
        // Simple default
    }

    [[nodiscard]] const Comparator* user_comparator() const { return user_comparator_; }

private:
    const Comparator* user_comparator_;
};

} // namespace lsmforge
