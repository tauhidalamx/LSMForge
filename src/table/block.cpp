#include "table/block.h"
#include "util/coding.h"
#include "util/crc32.h"
#include <algorithm>
#include <cassert>

namespace lsmforge {

// BlockHandle serialization
void BlockHandle::EncodeTo(std::string* dst) const {
    assert(offset_ != ~static_cast<uint64_t>(0));
    assert(size_ != ~static_cast<uint64_t>(0));
    PutVarint64(dst, offset_);
    PutVarint64(dst, size_);
}

Status BlockHandle::DecodeFrom(Slice* input) {
    if (GetVarint64(input, &offset_) && GetVarint64(input, &size_)) {
        return Status::OK();
    }
    return Status::Corruption("bad block handle");
}

// Footer serialization
void Footer::EncodeTo(std::string* dst) const {
    const size_t original_size = dst->size();
    metaindex_handle_.EncodeTo(dst);
    index_handle_.EncodeTo(dst);
    dst->resize(original_size + 2 * BlockHandle::kMaxEncodedLength, 0); // Padding
    PutFixed64(dst, kTableMagicNumber);
}

Status Footer::DecodeFrom(Slice* input) {
    if (input->size() < kEncodedLength) {
        return Status::Corruption("not an sstable (file too short for footer)");
    }

    const char* magic_ptr = input->data() + kEncodedLength - 8;
    const uint64_t magic = DecodeFixed64(magic_ptr);
    if (magic != kTableMagicNumber) {
        return Status::Corruption("not an sstable (bad magic number)");
    }

    Status result = metaindex_handle_.DecodeFrom(input);
    if (result.ok()) {
        result = index_handle_.DecodeFrom(input);
    }
    if (result.ok()) {
        // Skip padding
        const char* end = magic_ptr + 8;
        *input = Slice(end, input->data() + input->size() - end);
    }
    return result;
}

Status ReadBlock(RandomAccessFile* file, const ReadOptions& options,
                 const BlockHandle& handle, BlockContents* result) {
    result->data = Slice();
    result->cachable = false;
    result->heap_allocated = false;

    // Read the block contents as well as the 5-byte trailer (1 byte compression + 4 byte CRC)
    size_t n = static_cast<size_t>(handle.size());
    char* buf = new char[n + kBlockTrailerSize];
    Slice contents;
    Status s = file->Read(handle.offset(), n + kBlockTrailerSize, &contents, buf);
    if (!s.ok()) {
        delete[] buf;
        return s;
    }
    if (contents.size() != n + kBlockTrailerSize) {
        delete[] buf;
        return Status::Corruption("truncated block read");
    }

    // Verify CRC
    const char* data = contents.data();
    if (options.verify_checksums) {
        const uint32_t crc = crc32::Unmask(DecodeFixed32(data + n + 1));
        const uint32_t actual = crc32::Value(data, n + 1);
        if (actual != crc) {
            delete[] buf;
            return Status::Corruption("block checksum mismatch");
        }
    }

    if (data != buf) {
        delete[] buf;
        result->data = Slice(data, n);
        result->heap_allocated = false;
        result->cachable = false;
    } else {
        result->data = Slice(buf, n);
        result->heap_allocated = true;
        result->cachable = true;
    }
    return Status::OK();
}

// Block implementation
Block::Block(BlockContents contents)
    : contents_(std::move(contents)),
      data_(contents_.data.data()),
      size_(contents_.data.size()),
      restart_offset_(0) {
    if (size_ < sizeof(uint32_t)) {
        size_ = 0; // Error marker
    } else {
        size_t max_restarts_allowed = (size_ - sizeof(uint32_t)) / sizeof(uint32_t);
        uint32_t num_restarts = DecodeFixed32(data_ + size_ - sizeof(uint32_t));
        if (num_restarts > max_restarts_allowed) {
            size_ = 0; // Bad restart array length
        } else {
            restart_offset_ = static_cast<uint32_t>(size_ - (1 + num_restarts) * sizeof(uint32_t));
        }
    }
}

class Block::Iter : public Iterator {
public:
    Iter(const Comparator* comparator, const char* data, uint32_t restarts, uint32_t num_restarts)
        : comparator_(comparator),
          data_(data),
          restarts_(restarts),
          num_restarts_(num_restarts),
          current_(restarts_),
          restart_index_(num_restarts_) {
        assert(num_restarts_ > 0);
    }

    ~Iter() override = default;

    [[nodiscard]] bool Valid() const override { return current_ < restarts_; }
    [[nodiscard]] Status status() const override { return status_; }
    [[nodiscard]] Slice key() const override {
        assert(Valid());
        return Slice(key_);
    }
    [[nodiscard]] Slice value() const override {
        assert(Valid());
        return value_;
    }

    void Next() override {
        assert(Valid());
        ParseNextKey();
    }

    void Prev() override {
        assert(Valid());
        // Scan backwards to a restart point before current_
        const uint32_t original = current_;
        while (GetRestartPoint(restart_index_) >= original) {
            if (restart_index_ == 0) {
                // No more keys
                current_ = restarts_;
                restart_index_ = num_restarts_;
                return;
            }
            restart_index_--;
        }

        SeekToRestartPoint(restart_index_);
        do {
            // Loop until end of current entry reaches original
        } while (ParseNextKey() && NextEntryOffset() < original);
    }

    void Seek(const Slice& target) override {
        // Binary search in restart array to find the last restart point with a key <= target
        uint32_t left = 0;
        uint32_t right = num_restarts_ - 1;
        while (left < right) {
            uint32_t mid = (left + right + 1) / 2;
            uint32_t region_offset = GetRestartPoint(mid);
            uint32_t shared, non_shared, value_length;
            const char* key_ptr = DecodeEntry(data_ + region_offset, data_ + restarts_,
                                              &shared, &non_shared, &value_length);
            if (key_ptr == nullptr || shared != 0) {
                CorruptionError();
                return;
            }
            Slice mid_key(key_ptr, non_shared);
            if (comparator_->Compare(mid_key, target) < 0) {
                // Key at "mid" is smaller than "target". Therefore all
                // blocks before "mid" are uninteresting.
                left = mid;
            } else {
                // Key at "mid" is >= "target". Therefore all blocks
                // after "mid" are uninteresting.
                right = mid - 1;
            }
        }

        // Linear scan from restart point
        SeekToRestartPoint(left);
        while (true) {
            if (!ParseNextKey()) {
                return;
            }
            if (comparator_->Compare(key_, target) >= 0) {
                return;
            }
        }
    }

    void SeekToFirst() override {
        SeekToRestartPoint(0);
        ParseNextKey();
    }

    void SeekToLast() override {
        SeekToRestartPoint(num_restarts_ - 1);
        while (ParseNextKey() && NextEntryOffset() < restarts_) {
            // Keep going
        }
    }

private:
    void CorruptionError() {
        current_ = restarts_;
        restart_index_ = num_restarts_;
        status_ = Status::Corruption("bad entry in block");
        key_.clear();
        value_.clear();
    }

    [[nodiscard]] uint32_t GetRestartPoint(uint32_t index) const {
        assert(index < num_restarts_);
        return DecodeFixed32(data_ + restarts_ + index * sizeof(uint32_t));
    }

    void SeekToRestartPoint(uint32_t index) {
        key_.clear();
        restart_index_ = index;
        uint32_t offset = GetRestartPoint(index);
        value_ = Slice(data_ + offset, 0);
        current_ = offset;
    }

    [[nodiscard]] uint32_t NextEntryOffset() const {
        return static_cast<uint32_t>((value_.data() + value_.size()) - data_);
    }

    static const char* DecodeEntry(const char* p, const char* limit,
                                   uint32_t* shared, uint32_t* non_shared,
                                   uint32_t* value_length) {
        if (limit - p < 3) return nullptr;
        *shared = reinterpret_cast<const uint8_t*>(p)[0];
        *non_shared = reinterpret_cast<const uint8_t*>(p)[1];
        *value_length = reinterpret_cast<const uint8_t*>(p)[2];
        if ((*shared | *non_shared | *value_length) < 128) {
            // Fast path: all three values are single-byte integers
            p += 3;
        } else {
            if ((p = GetVarint32Ptr(p, limit, shared)) == nullptr) return nullptr;
            if ((p = GetVarint32Ptr(p, limit, non_shared)) == nullptr) return nullptr;
            if ((p = GetVarint32Ptr(p, limit, value_length)) == nullptr) return nullptr;
        }
        if (static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)) {
            return nullptr;
        }
        return p;
    }

    bool ParseNextKey() {
        current_ = NextEntryOffset();
        const char* p = data_ + current_;
        const char* limit = data_ + restarts_;
        if (p >= limit) {
            // No more entries
            current_ = restarts_;
            restart_index_ = num_restarts_;
            return false;
        }

        // Decode next entry
        uint32_t shared, non_shared, value_length;
        p = DecodeEntry(p, limit, &shared, &non_shared, &value_length);
        if (p == nullptr || key_.size() < shared) {
            CorruptionError();
            return false;
        } else {
            key_.resize(shared);
            key_.append(p, non_shared);
            value_ = Slice(p + non_shared, value_length);
            while (restart_index_ + 1 < num_restarts_ &&
                   GetRestartPoint(restart_index_ + 1) <= current_) {
                ++restart_index_;
            }
            return true;
        }
    }

    const Comparator* const comparator_;
    const char* const data_;
    const uint32_t restarts_;
    const uint32_t num_restarts_;

    uint32_t current_;
    uint32_t restart_index_;
    std::string key_;
    Slice value_;
    Status status_;
};

Iterator* Block::NewIterator(const Comparator* comparator) {
    if (size_ < sizeof(uint32_t)) {
        return NewErrorIterator(Status::Corruption("bad block contents"));
    }
    const uint32_t num_restarts = DecodeFixed32(data_ + size_ - sizeof(uint32_t));
    if (num_restarts == 0) {
        return NewEmptyIterator();
    } else {
        return new Iter(comparator, data_, restart_offset_, num_restarts);
    }
}

} // namespace lsmforge
