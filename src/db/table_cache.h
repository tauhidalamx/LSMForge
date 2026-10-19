#pragma once

#include "lsmforge/options.h"
#include "lsmforge/status.h"
#include "table/table_reader.h"
#include "util/env.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lsmforge {

class TableCache {
public:
    TableCache(std::string dbname, const Options& options, int entries);
    ~TableCache() = default;

    TableCache(const TableCache&) = delete;
    TableCache& operator=(const TableCache&) = delete;

    // Return an iterator for the specified file number (the corresponding file
    // length must be exactly "file_size" bytes). If "tableptr" is
    // non-null, also sets "*tableptr" to point to the TableReader.
    Iterator* NewIterator(const ReadOptions& options,
                          uint64_t file_number,
                          uint64_t file_size,
                          TableReader** tableptr = nullptr);

    // If a seek to internal key "k" in specified file finds an entry,
    // calls (*handle_result)(arg, found_key, found_value).
    Status Get(const ReadOptions& options,
               uint64_t file_number,
               uint64_t file_size,
               const Slice& k,
               void* arg,
               void (*handle_result)(void*, const Slice&, const Slice&));

    // Evict any entry for the specified file number
    void Evict(uint64_t file_number);

    // Find table reader
    Status FindTable(uint64_t file_number, uint64_t file_size, std::shared_ptr<TableReader>* table);

private:
    const std::string dbname_;
    const Options options_;
    const int entries_;

    std::mutex mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<TableReader>> cache_;
};

} // namespace lsmforge
