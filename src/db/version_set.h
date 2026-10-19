#pragma once

#include "db/table_cache.h"
#include "db/version_edit.h"
#include "lsmforge/comparator.h"
#include "lsmforge/options.h"
#include "lsmforge/stats.h"
#include "memtable/memtable.h"
#include "wal/wal_writer.h"
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace lsmforge {

class Compaction;
class Version;
class VersionSet;

class Version {
public:
    struct GetStats {
        FileMetaData* seek_file{nullptr};
        int seek_file_level{-1};
    };

    explicit Version(VersionSet* vset);
    ~Version();

    Version(const Version&) = delete;
    Version& operator=(const Version&) = delete;

    void Ref();
    void Unref();

    void AddIterators(const ReadOptions& options, std::vector<Iterator*>* iters);

    Status Get(const ReadOptions& options, const LookupKey& k, std::string* value,
               GetStats* stats, DBStats* db_stats);

    [[nodiscard]] int NumLevelFiles(int level) const {
        assert(level >= 0 && level < kNumLevels);
        return static_cast<int>(files_[level].size());
    }

    [[nodiscard]] uint64_t NumLevelBytes(int level) const;

    [[nodiscard]] const std::vector<FileMetaData*>& files(int level) const {
        assert(level >= 0 && level < kNumLevels);
        return files_[level];
    }

private:
    friend class VersionSet;
    friend class VersionBuilder;
    friend class Compaction;

    VersionSet* vset_;
    Version* next_;
    Version* prev_;
    int refs_{0};

    // List of files per level
    std::vector<FileMetaData*> files_[kNumLevels];
};

class Compaction {
public:
    Compaction(const Options* options, int level);
    ~Compaction();

    [[nodiscard]] int level() const noexcept { return level_; }
    [[nodiscard]] VersionEdit* edit() noexcept { return &edit_; }

    [[nodiscard]] int num_input_files(int which) const {
        return static_cast<int>(inputs_[which].size());
    }
    [[nodiscard]] FileMetaData* input(int which, int i) const {
        return inputs_[which][i];
    }

    [[nodiscard]] uint64_t MaxOutputFileSize() const noexcept { return max_output_file_size_; }

    // Is this a trivial move of files from level to level+1?
    [[nodiscard]] bool IsTrivialMove() const;

    // Add all inputs to this compaction as delete operations in edit.
    void AddInputDeletions(VersionEdit* edit);

    // Returns true if the information we have guarantees that "user_key"
    // is absent in levels > level_ + 1.
    [[nodiscard]] bool IsBaseLevelForKey(const Slice& user_key);

    void ReleaseInputs();

private:
    friend class Version;
    friend class VersionSet;

    int level_;
    uint64_t max_output_file_size_;
    Version* input_version_{nullptr};
    VersionEdit edit_;

    // Each compaction reads inputs from "level_" and "level_+1"
    std::vector<FileMetaData*> inputs_[2];
};

class VersionSet {
public:
    VersionSet(std::string dbname, const Options* options,
               TableCache* table_cache, const InternalKeyComparator* cmp);
    ~VersionSet();

    VersionSet(const VersionSet&) = delete;
    VersionSet& operator=(const VersionSet&) = delete;

    // Apply *edit to the current version to form a new descriptor that
    // is both saved to persistent state and installed as the new
    // current version. Will release *mu while actually writing to the file.
    // REQUIRES: *mu is held on entry.
    // REQUIRES: no other thread concurrently calls LogAndApply()
    Status LogAndApply(VersionEdit* edit, std::mutex* mu);

    // Recover the last saved descriptor from persistent storage.
    Status Recover();

    // Current version
    [[nodiscard]] Version* current() const noexcept { return current_; }

    // Sequence numbers and file numbers
    [[nodiscard]] uint64_t ManifestFileNumber() const noexcept { return manifest_file_number_; }
    [[nodiscard]] uint64_t NextFileNumber() const noexcept { return next_file_number_; }
    uint64_t NewFileNumber() noexcept { return next_file_number_++; }
    void ReuseFileNumber(uint64_t file_number) noexcept {
        if (next_file_number_ == file_number + 1) {
            next_file_number_ = file_number;
        }
    }

    [[nodiscard]] SequenceNumber LastSequence() const noexcept { return last_sequence_; }
    void SetLastSequence(SequenceNumber s) noexcept {
        assert(s >= last_sequence_);
        last_sequence_ = s;
    }

    [[nodiscard]] uint64_t LogNumber() const noexcept { return log_number_; }
    [[nodiscard]] uint64_t PrevLogNumber() const noexcept { return prev_log_number_; }

    // Return the number of Table files at the specified level.
    [[nodiscard]] int NumLevelFiles(int level) const { return current_->NumLevelFiles(level); }

    // Return the combined byte size of all files at the specified level.
    [[nodiscard]] uint64_t NumLevelBytes(int level) const { return current_->NumLevelBytes(level); }

    // Pick compaction
    Compaction* PickCompaction();

    // Range compaction
    Compaction* CompactRange(int level, const Slice* begin, const Slice* end);

    // Create iterator over all inputs in compaction
    Iterator* MakeInputIterator(Compaction* c);

    // Add all live files in all versions to the set
    void AddLiveFiles(std::set<uint64_t>* live);

    [[nodiscard]] TableCache* table_cache() const noexcept { return table_cache_; }
    [[nodiscard]] const InternalKeyComparator& icmp() const noexcept { return icmp_; }

private:
    class Builder;

    void AppendVersion(Version* v);
    void SetupOtherInputs(Compaction* c);

    Env* env_;
    const std::string dbname_;
    const Options* options_;
    TableCache* table_cache_;
    const InternalKeyComparator icmp_;
    uint64_t next_file_number_{2};
    uint64_t manifest_file_number_{0};
    SequenceNumber last_sequence_{0};
    uint64_t log_number_{0};
    uint64_t prev_log_number_{0};

    std::unique_ptr<WritableFile> descriptor_file_;
    std::unique_ptr<wal::WalWriter> descriptor_log_;
    Version dummy_versions_; // Head of circular doubly-linked list of versions
    Version* current_{nullptr};

    // Compaction scores
    double compaction_score_{-1.0};
    int compaction_level_{-1};
};

} // namespace lsmforge
