#pragma once

#include "lsmforge/comparator.h"
#include "lsmforge/iterator.h"

namespace lsmforge {

// Return an iterator that provided the union of the data in
// children[0,n-1]. Takes ownership of the child iterators and
// will delete them when the result iterator is deleted.
//
// The result does no duplicate suppression or tombstone handling
// (that is handled by DB-level iterators).
Iterator* NewMergingIterator(const Comparator* comparator, Iterator** children, int n);

} // namespace lsmforge
