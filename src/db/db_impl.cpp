#include "db/db_impl.h"
#include "db/db_iter.h"
#include "db/filename.h"
#include "lsmforge/comparator.h"
#include "table/merging_iterator.h"
#include "table/table_builder.h"
#include "util/coding.h"
#include "wal/wal_reader.h"
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lsmforge {

// Default Options constructor
Options::Options() : comparator(BytewiseComparator()) {}

// Default Comparator implementation
namespace {

class BytewiseComparatorImpl : public Comparator {
public:
    BytewiseComparatorImpl() = default;

    [[nodiscard]] const char* Name() const override {
        return "lsmforge.BytewiseComparator";
    }

    [[nodiscard]] int Compare(const Slice& a, const Slice& b) const override {
        return a.compare(b);
    }

    void FindShortestSeparator(std::string* start, const Slice& limit) const override {
        // Find length of common prefix
        size_t min_length = std::min(start->size(), limit.size());
        size_t diff_index = 0;
        while ((diff_index < min_length) &&
               ((*start)[diff_index] == limit[diff_index])) {
            diff_index++;
        }

        if (diff_index < min_length) {
            uint8_t diff_byte = static_cast<uint8_t>((*start)[diff_index]);
            if (diff_byte < 0xff && diff_byte + 1 < static_cast<uint8_t>(limit[diff_index])) {
                (*start)[diff_index]++;
                start->resize(diff_index + 1);
                assert(Compare(*start, limit) < 0);
            }
        }
    }

    void FindShortSuccessor(std::string* key) const override {
        // Find first character that can be incremented
        size_t n = key->size();
        for (size_t i = 0; i < n; i++) {
            const uint8_t byte = static_cast<uint8_t>((*key)[i]);
            if (byte != 0xff) {
                (*key)[i] = static_cast<char>(byte + 1);
                key->resize(i + 1);
                return;
            }
        }
    }
};

const BytewiseComparatorImpl kBytewiseComparator;

} // namespace

const Comparator* BytewiseComparator() {
    return &kBytewiseComparator;
}

// Iterator implementations
Iterator::~Iterator() {
    if (!cleanup_head_.IsEmpty()) {
        cleanup_head_.Run();
        for (CleanupNode* node = cleanup_head_.next; node != nullptr;) {
            node->Run();
            CleanupNode* next_node = node->next;
            delete node;
            node = next_node;
        }
    }
}

void Iterator::RegisterCleanup(CleanupFunction function, void* arg1, void* arg2) {
    assert(function != nullptr);
    CleanupNode* node;
    if (cleanup_head_.IsEmpty()) {
        node = &cleanup_head_;
    } else {
        node = new CleanupNode();
        node->next = cleanup_head_.next;
        cleanup_head_.next = node;
    }
    node->function = function;
    node->arg1 = arg1;
    node->arg2 = arg2;
}

namespace {

class EmptyIterator : public Iterator {
public:
    explicit EmptyIterator(Status s) : status_(s) {}
    ~EmptyIterator() override = default;
    [[nodiscard]] bool Valid() const override { return false; }
    void Seek(const Slice&) override {}
    void SeekToFirst() override {}
    void SeekToLast() override {}
    void Next() override { assert(false); }
    void Prev() override { assert(false); }
    [[nodiscard]] Slice key() const override { assert(false); return Slice(); }
    [[nodiscard]] Slice value() const override { assert(false); return Slice(); }
    [[nodiscard]] Status status() const override { return status_; }
private:
    Status status_;
};

} // namespace

Iterator* NewEmptyIterator() {
    return new EmptyIterator(Status::OK());
}

Iterator* NewErrorIterator(const Status& status) {
    return new EmptyIterator(status);
}

std::string DBStats::ToString() const {
    std::ostringstream ss;
    ss << "=== LSMForge Database Engine Statistics ===\n"
       << "Operations:\n"
       << "  PUT operations:        " << num_puts.load() << "\n"
       << "  GET operations:        " << num_gets.load() << "\n"
       << "  DELETE operations:     " << num_deletes.load() << "\n"
       << "  SCAN operations:       " << num_scans.load() << "\n"
       << "Storage & Memory:\n"
       << "  WAL bytes written:     " << wal_bytes_written.load() << " B\n"
       << "  MemTable memory:       " << memtable_bytes.load() << " B\n"
       << "  Active SSTables:       " << sst_count.load() << "\n"
       << "  Total SSTable size:    " << sst_total_size.load() << " B\n"
       << "Bloom Filter & Blocks:\n"
       << "  Bloom filter useful:   " << bloom_filter_useful.load() << " (disk reads skipped)\n"
       << "  Bloom filter positive: " << bloom_filter_positive.load() << "\n"
       << "  Data block reads:      " << data_block_reads.load() << "\n"
       << "Pipeline Timings & Compaction:\n"
       << "  Flushes performed:     " << flush_count.load() << "\n"
       << "  Flush duration:        " << flush_duration_micros.load() << " us\n"
       << "  Compactions run:       " << compaction_count.load() << "\n"
       << "  Compaction duration:   " << compaction_duration_micros.load() << " us\n"
       << "  Compaction read:       " << compaction_bytes_read.load() << " B\n"
       << "  Compaction written:    " << compaction_bytes_written.load() << " B\n"
       << "Errors:\n"
       << "  Write errors:          " << write_errors.load() << "\n"
       << "  Read errors:           " << read_errors.load() << "\n"
       << "===========================================";
    return ss.str();
}

// Handler for inserting WriteBatch entries into MemTable
class MemTableInserter : public WriteBatch::Handler {
public:
    MemTableInserter(SequenceNumber seq, MemTable* mem)
        : sequence_(seq), mem_(mem) {}

    Status Put(const Slice& key, const Slice& value) override {
        mem_->Add(sequence_++, kTypeValue, key, value);
        return Status::OK();
    }

    Status Delete(const Slice& key) override {
        mem_->Add(sequence_++, kTypeDeletion, key, Slice());
        return Status::OK();
    }

private:
    SequenceNumber sequence_;
    MemTable* mem_;
};

namespace {
Options MakeTableOptions(const Options& options, const InternalKeyComparator* icmp) {
    Options opt = options;
    opt.comparator = icmp;
    return opt;
}
} // namespace

DBImpl::DBImpl(const Options& options, std::string dbname)
    : env_(Env::Default()),
      dbname_(std::move(dbname)),
      options_(options),
      internal_comparator_(options.comparator ? options.comparator : BytewiseComparator()),
      table_cache_(std::make_unique<TableCache>(dbname_, MakeTableOptions(options_, &internal_comparator_), options_.max_open_files)),
      versions_(std::make_unique<VersionSet>(dbname_, &options_, table_cache_.get(), &internal_comparator_)) {}

DBImpl::~DBImpl() {
    Close();
}

Status DBImpl::Close() {
    // 1. Notify background worker to stop
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (shutting_down_.exchange(true)) {
            return Status::OK(); // Already closed or closing
        }
        background_cv_.notify_all();
        background_work_finished_signal_.notify_all();
    }

    // 2. Join background thread
    if (background_thread_.joinable()) {
        background_thread_.join();
    }

    std::unique_lock<std::mutex> lock(mutex_);

    // 3. Flush any pending memtables if open
    if (imm_ != nullptr) {
        CompactMemTable();
    }
    if (mem_ != nullptr && mem_->ApproximateMemoryUsage() > 0) {
        imm_ = mem_;
        mem_ = nullptr;
        CompactMemTable();
    }

    if (mem_ != nullptr) {
        mem_->Unref();
        mem_ = nullptr;
    }
    if (imm_ != nullptr) {
        imm_->Unref();
        imm_ = nullptr;
    }

    log_writer_.reset();
    log_file_.reset();

    // 4. Release file lock
    if (db_lock_ != nullptr) {
        env_->UnlockFile(db_lock_);
        db_lock_ = nullptr;
    }

    return Status::OK();
}

SequenceNumber DBImpl::GetEarliestSnapshot() const {
    if (snapshots_.empty()) {
        return versions_->LastSequence();
    }
    return snapshots_.oldest()->number_;
}

const Snapshot* DBImpl::GetSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshots_.New(versions_->LastSequence());
}

void DBImpl::ReleaseSnapshot(const Snapshot* snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshots_.Delete(static_cast<const SnapshotImpl*>(snapshot));
}

Status DBImpl::Put(const WriteOptions& opt, const Slice& key, const Slice& value) {
    WriteBatch batch;
    batch.Put(key, value);
    return Write(opt, &batch);
}

Status DBImpl::Delete(const WriteOptions& opt, const Slice& key) {
    WriteBatch batch;
    batch.Delete(key);
    return Write(opt, &batch);
}

Status DBImpl::MakeRoomForWrite(std::unique_lock<std::mutex>* lock, bool force) {
    bool allow_delay = !force;
    Status s;
    while (true) {
        if (!allow_delay) {
            // Force rotation
        } else if (versions_->NumLevelFiles(0) >= options_.level0_stop_writes_trigger) {
            // Level-0 files too high: stall write until background compaction finishes
            background_work_finished_signal_.wait(*lock);
            continue;
        }

        if (!force && mem_->ApproximateMemoryUsage() <= options_.write_buffer_size) {
            // There is room in current MemTable
            break;
        } else if (force && mem_->ApproximateMemoryUsage() == 0) {
            // MemTable is empty, nothing to force flush
            break;
        } else if (imm_ != nullptr) {
            // Current MemTable full and previous flush still running: wait
            background_work_finished_signal_.wait(*lock);
        } else {
            // Attempt to switch to a new MemTable and trigger flush of old
            assert(versions_->PrevLogNumber() == 0);
            uint64_t new_log_number = versions_->NewFileNumber();
            std::unique_ptr<WritableFile> lfile;
            s = env_->NewWritableFile(LogFileName(dbname_, new_log_number), &lfile);
            if (!s.ok()) {
                versions_->ReuseFileNumber(new_log_number);
                break;
            }

            log_file_ = std::move(lfile);
            log_writer_ = std::make_unique<wal::WalWriter>(log_file_.get());
            logfile_number_ = new_log_number;

            imm_ = mem_;
            mem_ = new MemTable(internal_comparator_);
            mem_->Ref();
            force = false; // Do not force again

            MaybeScheduleCompaction();
        }
    }
    return s;
}

Status DBImpl::Write(const WriteOptions& options, WriteBatch* my_batch) {
    std::unique_lock<std::mutex> lock(mutex_);
    Status s = MakeRoomForWrite(&lock, my_batch == nullptr);
    if (!s.ok()) {
        stats_.write_errors.fetch_add(1, std::memory_order_relaxed);
        return s;
    }

    uint64_t last_sequence = versions_->LastSequence();
    my_batch->SetSequence(last_sequence + 1);
    last_sequence += my_batch->Count();
    versions_->SetLastSequence(last_sequence);

    // 1. Append batch to WAL
    s = log_writer_->AddRecord(my_batch->Data());
    if (s.ok()) {
        stats_.wal_bytes_written.fetch_add(my_batch->Data().size(), std::memory_order_relaxed);
        if (options.sync || options_.wal_sync_policy == WalSyncPolicy::kSyncAlways) {
            s = log_file_->Sync();
        }
    }
    if (!s.ok()) {
        stats_.write_errors.fetch_add(1, std::memory_order_relaxed);
        return s;
    }

    // 2. Insert updates into MemTable
    MemTableInserter inserter(my_batch->Sequence(), mem_);
    s = my_batch->Iterate(&inserter);
    if (!s.ok()) {
        stats_.write_errors.fetch_add(1, std::memory_order_relaxed);
        return s;
    }

    stats_.num_puts.fetch_add(my_batch->Count(), std::memory_order_relaxed);
    stats_.memtable_bytes.store(mem_->ApproximateMemoryUsage(), std::memory_order_relaxed);
    return Status::OK();
}

Status DBImpl::Get(const ReadOptions& options, const Slice& key, std::string* value) {
    Status s;
    std::unique_lock<std::mutex> lock(mutex_);

    SequenceNumber snapshot;
    if (options.snapshot != nullptr) {
        snapshot = static_cast<const SnapshotImpl*>(options.snapshot)->number_;
    } else {
        snapshot = versions_->LastSequence();
    }

    MemTable* mem = mem_;
    MemTable* imm = imm_;
    Version* current = versions_->current();

    mem->Ref();
    if (imm != nullptr) imm->Ref();
    current->Ref();

    // Release lock while searching in memory and on disk!
    lock.unlock();

    stats_.num_gets.fetch_add(1, std::memory_order_relaxed);

    LookupKey lkey(key, snapshot);
    if (mem->Get(lkey, value, &s)) {
        // Found in active memtable
    } else if (imm != nullptr && imm->Get(lkey, value, &s)) {
        // Found in immutable memtable
    } else {
        // Search SSTables
        Version::GetStats gstats;
        s = current->Get(options, lkey, value, &gstats, &stats_);
    }

    lock.lock();
    mem->Unref();
    if (imm != nullptr) imm->Unref();
    current->Unref();

    if (!s.ok() && !s.IsNotFound()) {
        stats_.read_errors.fetch_add(1, std::memory_order_relaxed);
    }
    return s;
}

Iterator* DBImpl::NewIterator(const ReadOptions& options) {
    std::unique_lock<std::mutex> lock(mutex_);
    stats_.num_scans.fetch_add(1, std::memory_order_relaxed);

    SequenceNumber snapshot;
    if (options.snapshot != nullptr) {
        snapshot = static_cast<const SnapshotImpl*>(options.snapshot)->number_;
    } else {
        snapshot = versions_->LastSequence();
    }

    std::vector<Iterator*> list;
    list.push_back(mem_->NewIterator());
    mem_->Ref();
    if (imm_ != nullptr) {
        list.push_back(imm_->NewIterator());
        imm_->Ref();
    }
    versions_->current()->AddIterators(options, &list);
    versions_->current()->Ref();

    Iterator* internal_iter = NewMergingIterator(&internal_comparator_, list.data(),
                                                 static_cast<int>(list.size()));

    // Cleanup hook when iterator is deleted
    struct CleanupArgs {
        MemTable* mem;
        MemTable* imm;
        Version* version;
        std::mutex* mu;
    };
    auto* args = new CleanupArgs{mem_, imm_, versions_->current(), &mutex_};

    internal_iter->RegisterCleanup([](void* a1, void*) {
        auto* ca = static_cast<CleanupArgs*>(a1);
        std::lock_guard<std::mutex> l(*ca->mu);
        ca->mem->Unref();
        if (ca->imm) ca->imm->Unref();
        ca->version->Unref();
        delete ca;
    }, args, nullptr);

    return NewDBIterator(options_.comparator ? options_.comparator : BytewiseComparator(),
                         internal_iter, snapshot);
}

Status DBImpl::WriteLevel0Table(MemTable* mem, VersionEdit* edit, Version* base) {
    const uint64_t start_time = env_->NowMicros();

    std::unique_ptr<Iterator> iter(mem->NewIterator());
    iter->SeekToFirst();
    if (!iter->Valid()) {
        // Nothing to flush from an empty memtable
        return Status::OK();
    }

    FileMetaData meta;
    meta.number = versions_->NewFileNumber();

    std::string tmp_name = TempFileName(dbname_, meta.number);
    std::string sst_name = TableFileName(dbname_, meta.number);

    std::unique_ptr<WritableFile> file;
    Status s = env_->NewWritableFile(tmp_name, &file);
    if (!s.ok()) {
        return s;
    }

    TableBuilder builder(MakeTableOptions(options_, &internal_comparator_), file.get());
    for (; iter->Valid(); iter->Next()) {
        builder.Add(iter->key(), iter->value());
    }

    s = iter->status();
    if (s.ok()) {
        s = builder.Finish();
    }
    if (s.ok()) {
        meta.file_size = builder.FileSize();
        meta.smallest = builder.SmallestKey();
        meta.largest = builder.LargestKey();
        assert(meta.file_size > 0);
        s = file->Sync();
    }
    if (s.ok()) {
        s = file->Close();
    }
    if (s.ok()) {
        s = env_->RenameFile(tmp_name, sst_name);
    }

    if (s.ok()) {
        if (builder.NumEntries() > 0) {
            edit->AddFile(0, meta.number, meta.file_size, meta.smallest, meta.largest);
            stats_.flush_count.fetch_add(1, std::memory_order_relaxed);
            stats_.sst_count.fetch_add(1, std::memory_order_relaxed);
            stats_.sst_total_size.fetch_add(meta.file_size, std::memory_order_relaxed);
            uint64_t duration = env_->NowMicros() - start_time;
            stats_.flush_duration_micros.fetch_add(duration, std::memory_order_relaxed);
        } else {
            env_->RemoveFile(sst_name);
        }
    } else {
        env_->RemoveFile(tmp_name);
    }

    return s;
}

Status DBImpl::CompactMemTable() {
    assert(imm_ != nullptr);
    VersionEdit edit;
    Version* base = versions_->current();
    base->Ref();

    Status s = WriteLevel0Table(imm_, &edit, base);
    base->Unref();

    if (s.ok() && shutting_down_.load()) {
        s = Status::IOError("Database is shutting down");
    }

    if (s.ok()) {
        edit.SetPrevLogNumber(0);
        edit.SetLogNumber(logfile_number_); // Earliest log still needed
        s = versions_->LogAndApply(&edit, &mutex_);
    }

    if (s.ok()) {
        // Commit success: drop immutable memtable
        imm_->Unref();
        imm_ = nullptr;
        DeleteObsoleteFiles();
    }

    return s;
}

void DBImpl::MaybeScheduleCompaction() {
    if (background_compaction_scheduled_ || shutting_down_.load()) {
        return;
    }
    if (imm_ == nullptr && versions_->PickCompaction() == nullptr) {
        return;
    }

    background_compaction_scheduled_ = true;
    background_cv_.notify_one();
}

void DBImpl::BackgroundCall() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!shutting_down_.load()) {
        if (imm_ != nullptr) {
            CompactMemTable();
            background_work_finished_signal_.notify_all();
        } else {
            Compaction* c = versions_->PickCompaction();
            if (c != nullptr) {
                CompactionEngine::DoCompactionWork(this, c);
                delete c;
                DeleteObsoleteFiles();
                background_work_finished_signal_.notify_all();
            } else {
                background_compaction_scheduled_ = false;
                background_cv_.wait_for(lock, std::chrono::milliseconds(100));
            }
        }
    }
}

Status DBImpl::Flush(const FlushOptions& options) {
    std::unique_lock<std::mutex> lock(mutex_);
    Status s = MakeRoomForWrite(&lock, true);
    if (!s.ok()) return s;

    if (options.wait) {
        while (imm_ != nullptr && s.ok()) {
            background_work_finished_signal_.wait(lock);
        }
    }
    return s;
}

Status DBImpl::CompactRange(const Slice* begin, const Slice* end) {
    Status s = Flush(FlushOptions());
    if (!s.ok()) return s;

    std::unique_lock<std::mutex> lock(mutex_);

    for (int level = 0; level < kNumLevels - 1; level++) {
        while (true) {
            Compaction* c = versions_->CompactRange(level, begin, end);
            if (c == nullptr) break;
            s = CompactionEngine::DoCompactionWork(this, c);
            delete c;
            DeleteObsoleteFiles();
            if (!s.ok()) break;
        }
        if (!s.ok()) break;
    }
    return s;
}

void DBImpl::DeleteObsoleteFiles() {
    std::set<uint64_t> live;
    versions_->AddLiveFiles(&live);

    // Keep active WAL
    std::vector<std::string> filenames;
    env_->GetChildren(dbname_ + "/sst", &filenames);
    for (const auto& fn : filenames) {
        uint64_t number;
        FileType type;
        if (ParseFileName(fn, &number, &type) && type == kTableFile) {
            if (live.find(number) == live.end()) {
                table_cache_->Evict(number);
                env_->RemoveFile(TableFileName(dbname_, number));
            }
        }
    }

    filenames.clear();
    env_->GetChildren(dbname_ + "/wal", &filenames);
    for (const auto& fn : filenames) {
        uint64_t number;
        FileType type;
        if (ParseFileName(fn, &number, &type) && type == kLogFile) {
            if (number < versions_->LogNumber() && number != logfile_number_) {
                env_->RemoveFile(LogFileName(dbname_, number));
            }
        }
    }
}

Status DBImpl::RecoverLogFile(uint64_t log_number, SequenceNumber* max_sequence) {
    std::string fname = LogFileName(dbname_, log_number);
    std::unique_ptr<SequentialFile> file;
    Status status = env_->NewSequentialFile(fname, &file);
    if (!status.ok()) {
        return status;
    }

    wal::WalReader reader(file.get(), nullptr, true /* verify checksums */, 0);
    Slice record;
    std::string scratch;
    WriteBatch batch;
    MemTable* mem = nullptr;

    while (reader.ReadRecord(&record, &scratch)) {
        if (record.size() < 12) {
            // Partial trailing record or corrupt log
            break;
        }

        batch.Clear();
        // Re-assign raw data to batch
        std::string* rep = const_cast<std::string*>(&batch.Data());
        rep->assign(record.data(), record.size());

        if (mem == nullptr) {
            mem = new MemTable(internal_comparator_);
            mem->Ref();
        }

        MemTableInserter inserter(batch.Sequence(), mem);
        status = batch.Iterate(&inserter);
        if (!status.ok()) {
            break;
        }

        const SequenceNumber last_seq = batch.Sequence() + batch.Count() - 1;
        if (last_seq > *max_sequence) {
            *max_sequence = last_seq;
        }

        if (mem->ApproximateMemoryUsage() > options_.write_buffer_size) {
            VersionEdit edit;
            status = WriteLevel0Table(mem, &edit, versions_->current());
            if (!status.ok()) {
                mem->Unref();
                return status;
            }
            edit.SetLogNumber(log_number);
            status = versions_->LogAndApply(&edit, &mutex_);
            if (!status.ok()) {
                mem->Unref();
                return status;
            }
            mem->Unref();
            mem = nullptr;
        }
    }

    if (mem != nullptr) {
        VersionEdit edit;
        status = WriteLevel0Table(mem, &edit, versions_->current());
        if (status.ok()) {
            edit.SetPrevLogNumber(0);
            edit.SetLogNumber(log_number);
            status = versions_->LogAndApply(&edit, &mutex_);
        }
        mem->Unref();
        mem = nullptr;
    }

    return status;
}

Status DBImpl::Recover(SequenceNumber* max_sequence) {
    // 1. Create directory hierarchy if missing
    env_->CreateDirIfMissing(dbname_);
    env_->CreateDirIfMissing(dbname_ + "/wal");
    env_->CreateDirIfMissing(dbname_ + "/sst");
    env_->CreateDirIfMissing(dbname_ + "/tmp");

    // 2. Acquire database file lock
    Status s = env_->LockFile(LockFileName(dbname_), &db_lock_);
    if (!s.ok()) {
        return s;
    }

    // 3. Check for existing database
    if (!env_->FileExists(CurrentFileName(dbname_))) {
        if (!options_.create_if_missing) {
            return Status::InvalidArgument(dbname_, "does not exist (create_if_missing is false)");
        }
        // Create new database manifest
        VersionEdit new_db;
        new_db.SetComparatorName(options_.comparator->Name());
        new_db.SetLogNumber(0);
        new_db.SetNextFile(2);
        new_db.SetLastSequence(0);

        const std::string manifest = DescriptorFileName(dbname_, 1);
        std::unique_ptr<WritableFile> file;
        s = env_->NewWritableFile(manifest, &file);
        if (s.ok()) {
            wal::WalWriter writer(file.get());
            std::string record;
            new_db.EncodeTo(&record);
            s = writer.AddRecord(record);
            if (s.ok()) s = file->Sync();
        }
        if (s.ok()) {
            s = SetCurrentFile(env_, dbname_, 1);
        }
        if (!s.ok()) return s;
    } else {
        if (options_.error_if_exists) {
            return Status::InvalidArgument(dbname_, "exists (error_if_exists is true)");
        }
    }

    // 4. Recover VersionSet from Manifest
    s = versions_->Recover();
    if (!s.ok()) return s;

    // 5. Recover logs >= versions_->LogNumber()
    SequenceNumber max_seq = versions_->LastSequence();
    std::vector<std::string> filenames;
    env_->GetChildren(dbname_ + "/wal", &filenames);
    std::vector<uint64_t> logs;
    for (const auto& fn : filenames) {
        uint64_t number;
        FileType type;
        if (ParseFileName(fn, &number, &type) && type == kLogFile) {
            if (number >= versions_->LogNumber()) {
                logs.push_back(number);
            }
        }
    }

    std::sort(logs.begin(), logs.end());
    for (uint64_t log_num : logs) {
        s = RecoverLogFile(log_num, &max_seq);
        if (!s.ok()) return s;
    }

    if (max_seq > versions_->LastSequence()) {
        versions_->SetLastSequence(max_seq);
    }
    *max_sequence = versions_->LastSequence();

    return Status::OK();
}

Status DB::Open(const Options& options, const std::string& dbname, DB** dbptr) {
    *dbptr = nullptr;

    Options sanitized_options = options;
    if (sanitized_options.comparator == nullptr) {
        sanitized_options.comparator = BytewiseComparator();
    }

    auto impl = std::make_unique<DBImpl>(sanitized_options, dbname);
    impl->mutex_.lock();

    SequenceNumber max_sequence = 0;
    Status s = impl->Recover(&max_sequence);
    if (s.ok()) {
        uint64_t new_log_number = impl->versions_->NewFileNumber();
        std::unique_ptr<WritableFile> lfile;
        s = impl->env_->NewWritableFile(LogFileName(dbname, new_log_number), &lfile);
        if (s.ok()) {
            VersionEdit edit;
            edit.SetPrevLogNumber(0);
            edit.SetLogNumber(new_log_number);
            s = impl->versions_->LogAndApply(&edit, &impl->mutex_);
        }
        if (s.ok()) {
            impl->logfile_number_ = new_log_number;
            impl->log_file_ = std::move(lfile);
            impl->log_writer_ = std::make_unique<wal::WalWriter>(impl->log_file_.get());
            if (impl->mem_ == nullptr) {
                impl->mem_ = new MemTable(impl->internal_comparator_);
                impl->mem_->Ref();
            }
            impl->DeleteObsoleteFiles();
        }
    }

    if (s.ok()) {
        // Start background worker thread
        if (sanitized_options.max_background_jobs > 0) {
            impl->background_thread_ = std::thread(&DBImpl::BackgroundCall, impl.get());
        }
    }

    impl->mutex_.unlock();

    if (s.ok()) {
        *dbptr = impl.release();
    }
    return s;
}

Status DestroyDB(const std::string& dbname, const Options& options) {
    Env* env = Env::Default();
    std::vector<std::string> filenames;
    // Remove sst, wal, tmp, and manifest files
    std::vector<std::string> subdirs = {"/sst", "/wal", "/tmp"};
    for (const auto& sub : subdirs) {
        std::vector<std::string> files;
        if (env->GetChildren(dbname + sub, &files).ok()) {
            for (const auto& f : files) {
                env->RemoveFile(dbname + sub + "/" + f);
            }
            env->RemoveDir(dbname + sub);
        }
    }

    if (env->GetChildren(dbname, &filenames).ok()) {
        for (const auto& f : filenames) {
            env->RemoveFile(dbname + "/" + f);
        }
        env->RemoveDir(dbname);
    }
    return Status::OK();
}

std::vector<TableInfo> DBImpl::GetTableList() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    std::vector<TableInfo> res;
    if (!versions_ || !versions_->current()) return res;
    for (int lvl = 0; lvl < kNumLevels; lvl++) {
        for (const auto* f : versions_->current()->files(lvl)) {
            TableInfo t;
            t.number = f->number;
            t.level = lvl;
            t.file_size = f->file_size;
            t.smallest = (f->smallest.size() >= 8) ? ExtractUserKey(f->smallest).ToString() : f->smallest;
            t.largest = (f->largest.size() >= 8) ? ExtractUserKey(f->largest).ToString() : f->largest;
            res.push_back(std::move(t));
        }
    }
    return res;
}

std::vector<LevelInfo> DBImpl::GetLevelSummary() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    std::vector<LevelInfo> res;
    if (!versions_ || !versions_->current()) return res;
    for (int lvl = 0; lvl < kNumLevels; lvl++) {
        LevelInfo info;
        info.level = lvl;
        info.num_files = versions_->current()->files(lvl).size();
        info.total_bytes = versions_->current()->NumLevelBytes(lvl);
        res.push_back(info);
    }
    return res;
}

MemTableInfo DBImpl::GetMemTableInfo() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    MemTableInfo info;
    info.active_bytes = mem_ ? mem_->ApproximateMemoryUsage() : 0;
    info.immutable_count = imm_ ? 1 : 0;
    info.write_buffer_size = options_.write_buffer_size;
    return info;
}

std::vector<uint64_t> DBImpl::GetWalFiles() const {
    std::vector<std::string> filenames;
    env_->GetChildren(dbname_ + "/wal", &filenames);
    std::vector<uint64_t> result;
    for (const auto& fn : filenames) {
        uint64_t number;
        FileType type;
        if (ParseFileName(fn, &number, &type) && type == kLogFile) {
            result.push_back(number);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

Status RepairDB(const std::string& dbname, const Options& options) {
    return Status::UnsupportedOperation("RepairDB not implemented");
}

} // namespace lsmforge
