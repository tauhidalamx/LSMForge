#include "db/table_cache.h"
#include "db/filename.h"
#include <cassert>

namespace lsmforge {

TableCache::TableCache(std::string dbname, const Options& options, int entries)
    : dbname_(std::move(dbname)), options_(options), entries_(entries) {}

Status TableCache::FindTable(uint64_t file_number, uint64_t file_size,
                             std::shared_ptr<TableReader>* table) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(file_number);
    if (it != cache_.end()) {
        *table = it->second;
        return Status::OK();
    }

    std::string fname = TableFileName(dbname_, file_number);
    std::unique_ptr<RandomAccessFile> file;
    Status s = Env::Default()->NewRandomAccessFile(fname, &file);
    if (!s.ok()) {
        return s;
    }

    std::unique_ptr<TableReader> reader;
    s = TableReader::Open(options_, std::move(file), file_size, &reader);
    if (!s.ok()) {
        return s;
    }

    if (static_cast<int>(cache_.size()) >= entries_ && !cache_.empty()) {
        // Evict one entry
        cache_.erase(cache_.begin());
    }

    *table = std::shared_ptr<TableReader>(reader.release());
    cache_[file_number] = *table;
    return Status::OK();
}

Iterator* TableCache::NewIterator(const ReadOptions& options,
                                  uint64_t file_number,
                                  uint64_t file_size,
                                  TableReader** tableptr) {
    if (tableptr != nullptr) {
        *tableptr = nullptr;
    }
    std::shared_ptr<TableReader> table;
    Status s = FindTable(file_number, file_size, &table);
    if (!s.ok()) {
        return NewErrorIterator(s);
    }
    if (tableptr != nullptr) {
        *tableptr = table.get();
    }
    return table->NewIterator(options);
}

Status TableCache::Get(const ReadOptions& options,
                       uint64_t file_number,
                       uint64_t file_size,
                       const Slice& k,
                       void* arg,
                       void (*handle_result)(void*, const Slice&, const Slice&)) {
    std::shared_ptr<TableReader> table;
    Status s = FindTable(file_number, file_size, &table);
    if (!s.ok()) {
        return s;
    }

    std::unique_ptr<Iterator> iter(table->NewIterator(options));
    iter->Seek(k);
    if (iter->Valid()) {
        (*handle_result)(arg, iter->key(), iter->value());
    }
    return iter->status();
}

void TableCache::Evict(uint64_t file_number) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(file_number);
}

} // namespace lsmforge
