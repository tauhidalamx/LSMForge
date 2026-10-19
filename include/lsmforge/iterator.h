#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/status.h"

namespace lsmforge {

class Iterator {
public:
    Iterator() = default;
    virtual ~Iterator();

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    // An iterator is either positioned at a key/value pair, or
    // not valid. This method returns true iff the iterator is valid.
    [[nodiscard]] virtual bool Valid() const = 0;

    // Position at the first key in the source. The iterator is Valid()
    // after this call iff the source is not empty.
    virtual void SeekToFirst() = 0;

    // Position at the last key in the source. The iterator is Valid()
    // after this call iff the source is not empty.
    virtual void SeekToLast() = 0;

    // Position at the first key in the source that is at or past target.
    // The iterator is Valid() after this call iff the source contains
    // an entry that comes at or past target.
    virtual void Seek(const Slice& target) = 0;

    // Moves to the next entry in the source. After this call, Valid() is
    // true iff the iterator was not already at the last entry in the source.
    // REQUIRES: Valid()
    virtual void Next() = 0;

    // Moves to the previous entry in the source. After this call, Valid() is
    // true iff the iterator was not already at the first entry in the source.
    // REQUIRES: Valid()
    virtual void Prev() = 0;

    // Return the key for the current entry.
    // REQUIRES: Valid()
    [[nodiscard]] virtual Slice key() const = 0;

    // Return the value for the current entry.
    // REQUIRES: Valid()
    [[nodiscard]] virtual Slice value() const = 0;

    // If an error has occurred, return it. Else return an ok status.
    [[nodiscard]] virtual Status status() const = 0;

    using CleanupFunction = void (*)(void* arg1, void* arg2);
    void RegisterCleanup(CleanupFunction function, void* arg1, void* arg2);

private:
    struct CleanupNode {
        CleanupFunction function{nullptr};
        void* arg1{nullptr};
        void* arg2{nullptr};
        CleanupNode* next{nullptr};

        [[nodiscard]] bool IsEmpty() const noexcept { return function == nullptr; }
        void Run() {
            assert(function != nullptr);
            (*function)(arg1, arg2);
        }
    };
    CleanupNode cleanup_head_;
};

// Returns an empty iterator that returns status
Iterator* NewEmptyIterator();
Iterator* NewErrorIterator(const Status& status);

} // namespace lsmforge
