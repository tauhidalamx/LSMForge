#include "table/two_level_iterator.h"
#include <cassert>

namespace lsmforge {

namespace {

class TwoLevelIterator : public Iterator {
public:
    TwoLevelIterator(Iterator* index_iter, BlockFunction block_function,
                     void* arg, const ReadOptions& options)
        : block_function_(block_function),
          arg_(arg),
          options_(options),
          index_iter_(index_iter),
          data_iter_(nullptr) {}

    ~TwoLevelIterator() override {
        delete index_iter_;
        delete data_iter_;
    }

    void Seek(const Slice& target) override {
        index_iter_->Seek(target);
        InitDataBlock();
        if (data_iter_ != nullptr) data_iter_->Seek(target);
        SkipEmptyDataBlocksForward();
    }

    void SeekToFirst() override {
        index_iter_->SeekToFirst();
        InitDataBlock();
        if (data_iter_ != nullptr) data_iter_->SeekToFirst();
        SkipEmptyDataBlocksForward();
    }

    void SeekToLast() override {
        index_iter_->SeekToLast();
        InitDataBlock();
        if (data_iter_ != nullptr) data_iter_->SeekToLast();
        SkipEmptyDataBlocksBackward();
    }

    void Next() override {
        assert(Valid());
        data_iter_->Next();
        SkipEmptyDataBlocksForward();
    }

    void Prev() override {
        assert(Valid());
        data_iter_->Prev();
        SkipEmptyDataBlocksBackward();
    }

    [[nodiscard]] bool Valid() const override {
        return (data_iter_ != nullptr) && data_iter_->Valid();
    }

    [[nodiscard]] Slice key() const override {
        assert(Valid());
        return data_iter_->key();
    }

    [[nodiscard]] Slice value() const override {
        assert(Valid());
        return data_iter_->value();
    }

    [[nodiscard]] Status status() const override {
        if (!index_iter_->status().ok()) {
            return index_iter_->status();
        } else if (data_iter_ != nullptr && !data_iter_->status().ok()) {
            return data_iter_->status();
        } else {
            return status_;
        }
    }

private:
    void SaveError(const Status& s) {
        if (status_.ok() && !s.ok()) status_ = s;
    }

    void SkipEmptyDataBlocksForward() {
        while (data_iter_ == nullptr || !data_iter_->Valid()) {
            // Move to next block
            if (!index_iter_->Valid()) {
                SetDataIterator(nullptr);
                return;
            }
            index_iter_->Next();
            InitDataBlock();
            if (data_iter_ != nullptr) data_iter_->SeekToFirst();
        }
    }

    void SkipEmptyDataBlocksBackward() {
        while (data_iter_ == nullptr || !data_iter_->Valid()) {
            // Move to previous block
            if (!index_iter_->Valid()) {
                SetDataIterator(nullptr);
                return;
            }
            index_iter_->Prev();
            InitDataBlock();
            if (data_iter_ != nullptr) data_iter_->SeekToLast();
        }
    }

    void SetDataIterator(Iterator* data_iter) {
        if (data_iter_ != nullptr) {
            SaveError(data_iter_->status());
            delete data_iter_;
        }
        data_iter_ = data_iter;
    }

    void InitDataBlock() {
        if (!index_iter_->Valid()) {
            SetDataIterator(nullptr);
        } else {
            Slice handle = index_iter_->value();
            if (data_iter_ != nullptr && handle.compare(data_block_handle_) == 0) {
                // data_iter_ is already pointing to this block, no change needed
            } else {
                Iterator* iter = (*block_function_)(arg_, options_, handle);
                data_block_handle_.assign(handle.data(), handle.size());
                SetDataIterator(iter);
            }
        }
    }

    BlockFunction block_function_;
    void* arg_;
    const ReadOptions options_;
    Status status_;
    Iterator* index_iter_;
    Iterator* data_iter_;
    std::string data_block_handle_;
};

} // namespace

Iterator* NewTwoLevelIterator(Iterator* index_iter, BlockFunction block_function,
                             void* arg, const ReadOptions& options) {
    return new TwoLevelIterator(index_iter, block_function, arg, options);
}

} // namespace lsmforge
