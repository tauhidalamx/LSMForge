#include "db/db_iter.h"
#include <cassert>

namespace lsmforge {

namespace {

class DBIter : public Iterator {
public:
    enum Direction { kForward, kReverse };

    DBIter(const Comparator* cmp, Iterator* iter, SequenceNumber s)
        : user_comparator_(cmp),
          iter_(iter),
          sequence_(s),
          direction_(kForward),
          valid_(false) {}

    ~DBIter() override {
        delete iter_;
    }

    [[nodiscard]] bool Valid() const override { return valid_; }
    [[nodiscard]] Slice key() const override {
        assert(valid_);
        return Slice(saved_key_);
    }
    [[nodiscard]] Slice value() const override {
        assert(valid_);
        return Slice(saved_value_);
    }
    [[nodiscard]] Status status() const override {
        if (status_.ok()) {
            return iter_->status();
        } else {
            return status_;
        }
    }

    void Next() override {
        assert(valid_);
        if (direction_ == kReverse) {
            direction_ = kForward;
            if (!iter_->Valid()) {
                iter_->SeekToFirst();
            } else {
                iter_->Next();
            }
            if (!iter_->Valid()) {
                valid_ = false;
                saved_key_.clear();
                return;
            }
        }

        // We already know saved_key_, skip all remaining entries with this user key
        FindNextUserEntry(true, &saved_key_);
    }

    void Prev() override {
        assert(valid_);
        if (direction_ == kForward) {
            // Need to scan backward past all versions of current key
            direction_ = kReverse;
        }
        FindPrevUserEntry();
    }

    void Seek(const Slice& target) override {
        direction_ = kForward;
        saved_key_.clear();
        std::string ikey;
        AppendInternalKey(&ikey, ParsedInternalKey(target, sequence_, kTypeValueTypeForSeek));
        iter_->Seek(ikey);
        FindNextUserEntry(false, &saved_key_);
    }

    void SeekToFirst() override {
        direction_ = kForward;
        saved_key_.clear();
        iter_->SeekToFirst();
        FindNextUserEntry(false, &saved_key_);
    }

    void SeekToLast() override {
        direction_ = kReverse;
        saved_key_.clear();
        iter_->SeekToLast();
        FindPrevUserEntry();
    }

private:
    void FindNextUserEntry(bool skipping, std::string* skip) {
        assert(direction_ == kForward);
        valid_ = false;
        while (iter_->Valid()) {
            ParsedInternalKey ikey;
            Slice key_slice = iter_->key();
            if (!ParseInternalKey(key_slice, &ikey)) {
                status_ = Status::Corruption("corrupted internal key in DBIter");
                valid_ = false;
                return;
            }

            if (ikey.sequence <= sequence_) {
                if (skipping && user_comparator_->Compare(ikey.user_key, *skip) <= 0) {
                    // Entry has already been seen or is suppressed
                } else {
                    skipping = false;
                    switch (ikey.type) {
                        case kTypeDeletion:
                            // Suppress this deleted key and any older versions
                            skip->assign(ikey.user_key.data(), ikey.user_key.size());
                            skipping = true;
                            break;
                        case kTypeValue:
                            valid_ = true;
                            saved_key_.assign(ikey.user_key.data(), ikey.user_key.size());
                            saved_value_.assign(iter_->value().data(), iter_->value().size());
                            return;
                    }
                }
            }
            iter_->Next();
        }
    }

    void FindPrevUserEntry() {
        assert(direction_ == kReverse);
        valid_ = false;
        ValueType value_type = kTypeDeletion;
        if (iter_->Valid()) {
            do {
                ParsedInternalKey ikey;
                if (!ParseInternalKey(iter_->key(), &ikey)) {
                    status_ = Status::Corruption("corrupted internal key in DBIter");
                    return;
                }
                if (ikey.sequence <= sequence_) {
                    if (value_type != kTypeDeletion &&
                        user_comparator_->Compare(ikey.user_key, saved_key_) < 0) {
                        // We encountered an older user key! The previous key is complete.
                        break;
                    }
                    value_type = ikey.type;
                    if (value_type == kTypeDeletion) {
                        saved_key_.clear();
                        saved_value_.clear();
                    } else {
                        saved_key_.assign(ikey.user_key.data(), ikey.user_key.size());
                        saved_value_.assign(iter_->value().data(), iter_->value().size());
                    }
                }
                iter_->Prev();
            } while (iter_->Valid());
        }

        if (value_type == kTypeDeletion) {
            valid_ = false;
            saved_key_.clear();
            saved_value_.clear();
            direction_ = kForward;
        } else {
            valid_ = true;
        }
    }

    const Comparator* const user_comparator_;
    Iterator* const iter_;
    const SequenceNumber sequence_;
    Direction direction_;
    bool valid_;
    std::string saved_key_;
    std::string saved_value_;
    Status status_;
};

} // namespace

Iterator* NewDBIterator(const Comparator* user_key_comparator,
                        Iterator* internal_iter,
                        SequenceNumber sequence) {
    return new DBIter(user_key_comparator, internal_iter, sequence);
}

} // namespace lsmforge
