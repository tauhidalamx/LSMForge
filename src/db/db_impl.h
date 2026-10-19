#pragma once

#include "db/compaction.h"
#include "db/table_cache.h"
#include "db/version_set.h"
#include "lsmforge/db.h"
#include "lsmforge/stats.h"
#include "memtable/memtable.h"
#include "wal/wal_writer.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace lsmforge {

class SnapshotImpl : public Snapshot {
public:
    SequenceNumber number_{0};
    SnapshotImpl* prev_{nullptr};
    SnapshotImpl* next_{nullptr};

    [[nodiscard]] SequenceNumber GetSequenceNumber() const override { return number_; }
};

class SnapshotList {
public:
    SnapshotList() {
        list_.prev_ = &list_;
        list_.next_ = &list_;
    }

    [[nodiscard]] bool empty() const noexcept { return list_.next_ == &list_; }
    [[nodiscard]] SnapshotImpl* oldest() const noexcept {
        assert(!empty());
        return list_.next_;
    }
    [[nodiscard]] SnapshotImpl* newest() const noexcept {
        assert(!empty());
        return list_.prev_;
    }

    SnapshotImpl* New(SequenceNumber seq) {
        auto* s = new SnapshotImpl();
        s->number_ = seq;
        s->next_ = &list_;
        s->prev_ = list_.prev_;
        s->prev_->next_ = s;
        s->next_->prev_ = s;
        return s;
    }

    void Delete(const SnapshotImpl* s) {
        auto* snap = const_cast<SnapshotImpl*>(s);
        snap->prev_->next_ = snap->next_;
        snap->next_->prev_ = snap->prev_;
        delete snap;
    }

private:
    SnapshotImpl list_;
};

class DBImpl : public DB {
public:
    DBImpl(const Options& options, std::string dbname);
    ~DBImpl() override;

    Status Put(const WriteOptions& options, const Slice& key, const Slice& value) override;
    Status Delete(const WriteOptions& options, const Slice& key) override;
    Status Write(const WriteOptions& options, WriteBatch* updates) override;
    Status Get(const ReadOptions& options, const Slice& key, std::string* value) override;
    [[nodiscard]] Iterator* NewIterator(const ReadOptions& options) override;

    [[nodiscard]] const Snapshot* GetSnapshot() override;
    void ReleaseSnapshot(const Snapshot* snapshot) override;

    Status Flush(const FlushOptions& options) override;
    Status CompactRange(const Slice* begin, const Slice* end) override;

    [[nodiscard]] DBStats GetStats() const override { return stats_; }
    Status Close() override;

    [[nodiscard]] std::vector<TableInfo> GetTableList() const override;
    [[nodiscard]] std::vector<LevelInfo> GetLevelSummary() const override;
    [[nodiscard]] MemTableInfo GetMemTableInfo() const override;
    [[nodiscard]] std::vector<uint64_t> GetWalFiles() const override;
    [[nodiscard]] const Options& GetOptions() const override { return options_; }
    [[nodiscard]] const std::string& GetName() const override { return dbname_; }

    // Background workers and compaction execution
    void BackgroundCall();
    void BackgroundCompaction();
    Status CompactMemTable();

    // Table building helper
    Status WriteLevel0Table(MemTable* mem, VersionEdit* edit, Version* base);

    [[nodiscard]] const Options& options() const noexcept { return options_; }
    [[nodiscard]] VersionSet* versions() noexcept { return versions_.get(); }
    std::mutex& mutex() noexcept { return mutex_; }
    DBStats& stats() noexcept { return stats_; }
    [[nodiscard]] const std::string& dbname() const noexcept { return dbname_; }
    [[nodiscard]] SequenceNumber GetEarliestSnapshot() const;

private:
    friend class DB;
    friend class CompactionEngine;

    // Internal recovery routines
    Status Recover(SequenceNumber* max_sequence);
    Status RecoverLogFile(uint64_t log_number, SequenceNumber* max_sequence);

    // Write flow
    Status MakeRoomForWrite(std::unique_lock<std::mutex>* lock, bool force);
    void MaybeScheduleCompaction();
    void DeleteObsoleteFiles();

    Env* env_;
    const std::string dbname_;
    const Options options_;
    const InternalKeyComparator internal_comparator_;

    std::mutex mutex_;
    std::condition_variable background_work_finished_signal_;
    std::condition_variable background_cv_;
    std::thread background_thread_;
    std::atomic<bool> shutting_down_{false};
    bool background_compaction_scheduled_{false};

    FileLock* db_lock_{nullptr};

    std::unique_ptr<TableCache> table_cache_;
    std::unique_ptr<VersionSet> versions_;

    // Active MemTable and WAL
    MemTable* mem_{nullptr};
    MemTable* imm_{nullptr}; // Immutable MemTable awaiting flush
    std::unique_ptr<WritableFile> log_file_;
    std::unique_ptr<wal::WalWriter> log_writer_;
    uint64_t logfile_number_{0};

    SnapshotList snapshots_;
    mutable DBStats stats_;
};

} // namespace lsmforge
