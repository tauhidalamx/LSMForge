#include "memtable/memtable.h"
#include <cassert>

namespace lsmforge {

static Slice GetLengthPrefixedSliceFromMemTableEntry(const char* data) {
    uint32_t len;
    const char* p = data;
    p = GetVarint32Ptr(p, p + 5, &len);
    return Slice(p, len);
}

MemTable::MemTable(const InternalKeyComparator& comparator)
    : comparator_(comparator),
      refs_(0),
      table_(comparator_, &arena_) {}

MemTable::~MemTable() {
    assert(refs_ == 0);
}

int MemTable::KeyComparator::operator()(const char* aptr, const char* bptr) const {
    // Internal keys are length prefixed slices
    Slice a = GetLengthPrefixedSliceFromMemTableEntry(aptr);
    Slice b = GetLengthPrefixedSliceFromMemTableEntry(bptr);
    return comparator.Compare(a, b);
}

// Encode a suitable format:
// [varint32(key_length + 8)] [user_key] [packed(seq, type)] [varint32(val_length)] [val]
void MemTable::Add(SequenceNumber seq, ValueType type, const Slice& key, const Slice& val) {
    const size_t key_size = key.size();
    const size_t val_size = val.size();
    const size_t internal_key_size = key_size + 8;
    const size_t encoded_len = 
        5 + internal_key_size + 5 + val_size; // Max 5 bytes for 32-bit varints
    char* buf = arena_.Allocate(encoded_len);

    char* p = EncodeVarint32(buf, static_cast<uint32_t>(internal_key_size));
    std::memcpy(p, key.data(), key_size);
    p += key_size;
    EncodeFixed64(p, PackSequenceAndType(seq, type));
    p += 8;
    p = EncodeVarint32(p, static_cast<uint32_t>(val_size));
    std::memcpy(p, val.data(), val_size);
    p += val_size;

    assert(static_cast<size_t>(p - buf) <= encoded_len);
    table_.Insert(buf);
}

bool MemTable::Get(const LookupKey& key, std::string* value, Status* s) {
    Slice mem_key = key.memtable_key();
    Table::Iterator iter(&table_);
    iter.Seek(mem_key.data());

    if (iter.Valid()) {
        // Entry format is:
        //    klength  : varint32
        //    userkey  : char[klength - 8]
        //    tag      : uint64
        //    vlength  : varint32
        //    value    : char[vlength]
        const char* entry = iter.key();
        uint32_t key_length;
        const char* key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);
        if (comparator_.comparator.user_comparator()->Compare(
                Slice(key_ptr, key_length - 8), key.user_key()) == 0) {
            // Correct user key! Check sequence number tag
            const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
            SequenceNumber entry_seq;
            ValueType entry_type;
            UnpackSequenceAndType(tag, &entry_seq, &entry_type);

            if (entry_type == kTypeValue) {
                Slice v = GetLengthPrefixedSliceFromMemTableEntry(key_ptr + key_length);
                value->assign(v.data(), v.size());
                *s = Status::OK();
                return true;
            } else if (entry_type == kTypeDeletion) {
                *s = Status::NotFound("Deleted");
                return true;
            }
        }
    }
    return false;
}

class MemTableIterator : public Iterator {
public:
    explicit MemTableIterator(MemTable::Table* table) : iter_(table) {}

    ~MemTableIterator() override = default;

    [[nodiscard]] bool Valid() const override { return iter_.Valid(); }

    void Seek(const Slice& k) override {
        // Form an encoded internal key with length prefix
        std::string encoded;
        PutVarint32(&encoded, static_cast<uint32_t>(k.size()));
        encoded.append(k.data(), k.size());
        iter_.Seek(encoded.data());
    }

    void SeekToFirst() override { iter_.SeekToFirst(); }
    void SeekToLast() override { iter_.SeekToLast(); }
    void Next() override { iter_.Next(); }
    void Prev() override { iter_.Prev(); }

    [[nodiscard]] Slice key() const override {
        return GetLengthPrefixedSliceFromMemTableEntry(iter_.key());
    }

    [[nodiscard]] Slice value() const override {
        Slice key_slice = GetLengthPrefixedSliceFromMemTableEntry(iter_.key());
        return GetLengthPrefixedSliceFromMemTableEntry(key_slice.data() + key_slice.size());
    }

    [[nodiscard]] Status status() const override { return Status::OK(); }

private:
    MemTable::Table::Iterator iter_;
};

Iterator* MemTable::NewIterator() {
    return new MemTableIterator(&table_);
}

LookupKey::LookupKey(const Slice& user_key, SequenceNumber s) {
    size_t usize = user_key.size();
    size_t needed = usize + 13; // 5 bytes varint32 + 8 bytes tag
    char* dst;
    if (needed <= sizeof(space_)) {
        dst = space_;
    } else {
        dst = new char[needed];
    }
    start_ = dst;
    dst = EncodeVarint32(dst, static_cast<uint32_t>(usize + 8));
    kstart_ = dst;
    std::memcpy(dst, user_key.data(), usize);
    dst += usize;
    EncodeFixed64(dst, PackSequenceAndType(s, kTypeValueTypeForSeek));
    dst += 8;
    end_ = dst;
}

LookupKey::~LookupKey() {
    if (start_ != space_) {
        delete[] start_;
    }
}

} // namespace lsmforge
