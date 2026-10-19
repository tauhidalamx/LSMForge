#pragma once

#include "lsmforge/iterator.h"
#include "lsmforge/options.h"
#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "table/block.h"
#include "table/bloom_filter.h"
#include "table/format.h"
#include "util/env.h"
#include <memory>
#include <string>

namespace lsmforge {

class TableReader {
public:
    // Attempt to open the table that is stored in bytes [0..size-1] of "file",
    // and read the metadata entries of the table.
    static Status Open(const Options& options,
                       std::unique_ptr<RandomAccessFile> file,
                       uint64_t size,
                       std::unique_ptr<TableReader>* table);

    ~TableReader();

    TableReader(const TableReader&) = delete;
    TableReader& operator=(const TableReader&) = delete;

    // Given a key, return an iterator that yields the corresponding value
    // or next greater key/value.
    [[nodiscard]] Iterator* NewIterator(const ReadOptions& options);

    // Tests if user_key may be present according to Bloom filter.
    [[nodiscard]] bool KeyMayMatch(const Slice& user_key) const;

    // Read data block for a given index entry
    static Iterator* BlockReader(void* arg, const ReadOptions& options, const Slice& index_value);

private:
    struct Rep;
    explicit TableReader(std::unique_ptr<Rep> rep);

    std::unique_ptr<Rep> rep_;
};

} // namespace lsmforge
