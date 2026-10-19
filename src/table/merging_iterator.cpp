#include "table/merging_iterator.h"
#include <cassert>
#include <vector>

namespace lsmforge {

namespace {

class MergingIterator : public Iterator {
public:
    MergingIterator(const Comparator* comparator, Iterator** children, int n)
        : comparator_(comparator),
          children_(children, children + n),
          current_(nullptr),
          direction_(kForward) {}

    ~MergingIterator() override {
        for (auto* child : children_) {
            delete child;
        }
    }

    [[nodiscard]] bool Valid() const override {
        return (current_ != nullptr);
    }

    void SeekToFirst() override {
        for (auto* child : children_) {
            child->SeekToFirst();
        }
        FindSmallest();
        direction_ = kForward;
    }

    void SeekToLast() override {
        for (auto* child : children_) {
            child->SeekToLast();
        }
        FindLargest();
        direction_ = kReverse;
    }

    void Seek(const Slice& target) override {
        for (auto* child : children_) {
            child->Seek(target);
        }
        FindSmallest();
        direction_ = kForward;
    }

    void Next() override {
        assert(Valid());

        // Ensure that all children are positioned after key()
        // If we were moving in the reverse direction, we need to reposition them
        if (direction_ != kForward) {
            for (auto* child : children_) {
                if (child != current_) {
                    child->Seek(key());
                    if (child->Valid() && comparator_->Compare(key(), child->key()) == 0) {
                        child->Next();
                    }
                }
            }
            direction_ = kForward;
        }

        current_->Next();
        FindSmallest();
    }

    void Prev() override {
        assert(Valid());

        // Ensure that all children are positioned before key()
        if (direction_ != kReverse) {
            for (auto* child : children_) {
                if (child != current_) {
                    child->Seek(key());
                    if (child->Valid()) {
                        // Child is at first entry >= key(). Step back one.
                        child->Prev();
                    } else {
                        // Child has no entries >= key(). Position at that child's last entry.
                        child->SeekToLast();
                    }
                }
            }
            direction_ = kReverse;
        }

        current_->Prev();
        FindLargest();
    }

    [[nodiscard]] Slice key() const override {
        assert(Valid());
        return current_->key();
    }

    [[nodiscard]] Slice value() const override {
        assert(Valid());
        return current_->value();
    }

    [[nodiscard]] Status status() const override {
        Status status;
        for (auto* child : children_) {
            status = child->status();
            if (!status.ok()) {
                break;
            }
        }
        return status;
    }

private:
    enum Direction {
        kForward,
        kReverse
    };

    void FindSmallest() {
        Iterator* smallest = nullptr;
        for (auto* child : children_) {
            if (child->Valid()) {
                if (smallest == nullptr) {
                    smallest = child;
                } else if (comparator_->Compare(child->key(), smallest->key()) < 0) {
                    smallest = child;
                }
            }
        }
        current_ = smallest;
    }

    void FindLargest() {
        Iterator* largest = nullptr;
        for (auto* child : children_) {
            if (child->Valid()) {
                if (largest == nullptr) {
                    largest = child;
                } else if (comparator_->Compare(child->key(), largest->key()) > 0) {
                    largest = child;
                }
            }
        }
        current_ = largest;
    }

    const Comparator* const comparator_;
    std::vector<Iterator*> children_;
    Iterator* current_;
    Direction direction_;
};

} // namespace

Iterator* NewMergingIterator(const Comparator* comparator, Iterator** children, int n) {
    assert(n >= 0);
    if (n == 0) {
        return NewEmptyIterator();
    } else if (n == 1) {
        return children[0];
    } else {
        return new MergingIterator(comparator, children, n);
    }
}

} // namespace lsmforge
