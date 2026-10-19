#pragma once

#include "lsmforge/options.h"
#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "table/block_builder.h"
#include "table/bloom_filter.h"
#include "table/format.h"
#include "util/env.h"
#include <cstdint>
#include <string>
#include <vector>

namespace lsmforge {

class TableBuilder {
public:
    TableBuilder(const Options& options, WritableFile* file);
    ~TableBuilder();

    TableBuilder(const TableBuilder&) = delete;
    TableBuilder& operator=(const TableBuilder&) = delete;

    // Add key,value to the table being constructed.
    // REQUIRES: key is after any previously added key according to comparator.
    // REQUIRES: Finish() or Abandon() has not been called.
    void Add(const Slice& key, const Slice& value);

    // Advanced: flush buffered key/value pairs to file.
    // Can be used to ensure that two keys are in different data blocks.
    void Flush();

    // Return non-ok status if some error occurred during building.
    [[nodiscard]] Status status() const { return status_; }

    // Finish building the table. Stops using the file passed to the
    // constructor after this function returns.
    // REQUIRES: Finish() or Abandon() has not been called.
    Status Finish();

    // Indicate that the contents of this builder should be abandoned.
    // Stops using the file passed to the constructor after this function returns.
    // If the caller is not going to call Finish(), it must call Abandon().
    void Abandon();

    // Number of calls to Add() so far.
    [[nodiscard]] uint64_t NumEntries() const noexcept { return num_entries_; }

    // Size of the file generated so far. If called after Finish(),
    // returns the size of the entire generated file.
    [[nodiscard]] uint64_t FileSize() const noexcept { return offset_; }

    [[nodiscard]] std::string SmallestKey() const { return smallest_key_; }
    [[nodiscard]] std::string LargestKey() const { return largest_key_; }

private:
    void WriteBlock(BlockBuilder* block, BlockHandle* handle);

    Options options_;
    WritableFile* file_;
    uint64_t offset_{0};
    Status status_;
    BlockBuilder data_block_;
    BlockBuilder index_block_;
    std::string last_key_;
    uint64_t num_entries_{0};
    bool closed_{false};

    // Bloom filter state
    std::unique_ptr<BloomFilterPolicy> filter_policy_;
    std::vector<std::string> filter_keys_;

    // Key ranges
    std::string smallest_key_;
    std::string largest_key_;
    BlockHandle pending_handle_;
    bool pending_index_entry_{false};
};

} // namespace lsmforge
