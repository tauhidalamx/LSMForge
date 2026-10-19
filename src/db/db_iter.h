#pragma once

#include "lsmforge/comparator.h"
#include "lsmforge/iterator.h"
#include "lsmforge/types.h"

namespace lsmforge {

class DBImpl;

// Return a new iterator that converts internal keys (userkey + seq + type)
// into user keys, skips tombstones, and filters entries past the specified sequence number.
Iterator* NewDBIterator(
    const Comparator* user_key_comparator,
    Iterator* internal_iter,
    SequenceNumber sequence);

} // namespace lsmforge
