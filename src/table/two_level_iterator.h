#pragma once

#include "lsmforge/iterator.h"
#include "lsmforge/options.h"

namespace lsmforge {

// Return a new two level iterator. A two-level iterator contains an
// index iterator and a data iterator. The index iterator yields a sequence
// of keys and values, where the values are handles that can be passed to
// "block_function" to yield a data iterator for a block of key/value pairs.
//
// Takes ownership of "index_iter" and will delete it when the returned
// iterator is deleted.
using BlockFunction = Iterator* (*)(void* arg, const ReadOptions& options, const Slice& index_value);

Iterator* NewTwoLevelIterator(
    Iterator* index_iter,
    BlockFunction block_function,
    void* arg,
    const ReadOptions& options);

} // namespace lsmforge
