#pragma once

#include "lsmforge/iterator.h"
#include "lsmforge/options.h"
#include "lsmforge/slice.h"
#include "lsmforge/snapshot.h"
#include "lsmforge/stats.h"
#include "lsmforge/status.h"
#include "lsmforge/write_batch.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace lsmforge {

struct TableInfo {
    uint64_t number{0};
    int level{0};
    uint64_t file_size{0};
    std::string smallest;
    std::string largest;
};

struct LevelInfo {
    int level{0};
    size_t num_files{0};
    uint64_t total_bytes{0};
};

struct MemTableInfo {
    size_t active_bytes{0};
    size_t active_entries{0};
    size_t immutable_count{0};
    size_t write_buffer_size{0};
};

class DB {
public:
    // Open the database with the specified "name".
    // Stores a pointer to a heap-allocated database in *dbptr and returns
    // OK on success.
    // Stores nullptr in *dbptr and returns a non-OK status on error.
    // Caller should delete *dbptr when it is no longer needed.
    static Status Open(const Options& options, const std::string& name, DB** dbptr);

    DB() = default;
    virtual ~DB() = default;

    DB(const DB&) = delete;
    DB& operator=(const DB&) = delete;

    // Set the database entry for "key" to "value". Returns OK on success,
    // and a non-OK status on error.
    virtual Status Put(const WriteOptions& options, const Slice& key, const Slice& value) = 0;

    // Remove the database entry for "key". Returns OK on success,
    // and a non-OK status on error. It is not an error if "key" did not exist.
    virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;

    // Apply the specified updates to the database atomically.
    // Returns OK on success, non-OK on failure.
    virtual Status Write(const WriteOptions& options, WriteBatch* updates) = 0;

    // If the database contains an entry for "key" store the
    // corresponding value in *value and return OK.
    //
    // If there is no entry for "key" leave *value unchanged and return
    // a status for which Status::IsNotFound() returns true.
    //
    // May return any of the other Status codes on error.
    virtual Status Get(const ReadOptions& options, const Slice& key, std::string* value) = 0;

    // Return a heap-allocated iterator over the contents of the database.
    // The result of NewIterator() is initially invalid (caller must
    // call one of the Seek methods on the iterator before using it).
    //
    // Caller should delete the iterator when it is no longer needed.
    // The returned iterator should not be used after this db has been closed.
    [[nodiscard]] virtual Iterator* NewIterator(const ReadOptions& options) = 0;

    // Return a handle to the current DB state. Iterators created with
    // this handle will all observe a stable snapshot of the current DB state.
    // The caller must call ReleaseSnapshot(result) when the snapshot is no
    // longer needed.
    [[nodiscard]] virtual const Snapshot* GetSnapshot() = 0;

    // Release a previously acquired snapshot. The caller must not
    // use "snapshot" after this call.
    virtual void ReleaseSnapshot(const Snapshot* snapshot) = 0;

    // Force flush of current active MemTable to Level 0 SSTable.
    virtual Status Flush(const FlushOptions& options = FlushOptions()) = 0;

    // Compact the underlying storage for keys in range [*begin, *end].
    // If begin == nullptr, compacts from the beginning of the database.
    // If end == nullptr, compacts through the end of the database.
    virtual Status CompactRange(const Slice* begin = nullptr, const Slice* end = nullptr) = 0;

    // Trigger full leveled compaction of all levels.
    virtual Status Compact() { return CompactRange(nullptr, nullptr); }

    // Retrieve database operational statistics.
    [[nodiscard]] virtual DBStats GetStats() const = 0;

    // Inspection and Observability APIs
    [[nodiscard]] virtual std::vector<TableInfo> GetTableList() const { return {}; }
    [[nodiscard]] virtual std::vector<LevelInfo> GetLevelSummary() const { return {}; }
    [[nodiscard]] virtual MemTableInfo GetMemTableInfo() const { return {}; }
    [[nodiscard]] virtual std::vector<uint64_t> GetWalFiles() const { return {}; }
    [[nodiscard]] virtual const Options& GetOptions() const = 0;
    [[nodiscard]] virtual const std::string& GetName() const = 0;

    // Gracefully close and flush the database.
    virtual Status Close() = 0;
};

// Destroy the contents of the specified database.
// Be very careful using this method.
Status DestroyDB(const std::string& name, const Options& options);

// Repair a corrupted database.
Status RepairDB(const std::string& name, const Options& options);

} // namespace lsmforge
