#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include "lsmforge/types.h"
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace lsmforge {

static constexpr int kNumLevels = 7;

struct FileMetaData {
    int refs{0};
    uint64_t number{0};
    uint64_t file_size{0};
    std::string smallest; // InternalKey
    std::string largest;  // InternalKey

    FileMetaData() = default;
};

class VersionEdit {
public:
    VersionEdit() = default;
    ~VersionEdit() = default;

    void Clear();

    void SetComparatorName(const Slice& name) {
        has_comparator_ = true;
        comparator_ = name.ToString();
    }
    void SetLogNumber(uint64_t num) {
        has_log_number_ = true;
        log_number_ = num;
    }
    void SetPrevLogNumber(uint64_t num) {
        has_prev_log_number_ = true;
        prev_log_number_ = num;
    }
    void SetNextFile(uint64_t num) {
        has_next_file_number_ = true;
        next_file_number_ = num;
    }
    void SetLastSequence(SequenceNumber seq) {
        has_last_sequence_ = true;
        last_sequence_ = seq;
    }

    // Add the specified file at the specified level.
    // REQUIRES: This version has not been saved (see VersionSet::Save)
    // REQUIRES: "smallest" and "largest" are internal keys
    void AddFile(int level, uint64_t file, uint64_t file_size,
                 const Slice& smallest, const Slice& largest) {
        FileMetaData f;
        f.number = file;
        f.file_size = file_size;
        f.smallest = smallest.ToString();
        f.largest = largest.ToString();
        new_files_.emplace_back(level, f);
    }

    // Delete the specified "file" from the specified "level".
    void DeleteFile(int level, uint64_t file) {
        deleted_files_.insert(std::make_pair(level, file));
    }

    void EncodeTo(std::string* dst) const;
    Status DecodeFrom(const Slice& src);

    [[nodiscard]] const std::string& comparator() const { return comparator_; }
    [[nodiscard]] bool has_comparator() const { return has_comparator_; }
    [[nodiscard]] uint64_t log_number() const { return log_number_; }
    [[nodiscard]] bool has_log_number() const { return has_log_number_; }
    [[nodiscard]] uint64_t prev_log_number() const { return prev_log_number_; }
    [[nodiscard]] bool has_prev_log_number() const { return has_prev_log_number_; }
    [[nodiscard]] uint64_t next_file_number() const { return next_file_number_; }
    [[nodiscard]] bool has_next_file_number() const { return has_next_file_number_; }
    [[nodiscard]] SequenceNumber last_sequence() const { return last_sequence_; }
    [[nodiscard]] bool has_last_sequence() const { return has_last_sequence_; }

    using DeletedFileSet = std::set<std::pair<int, uint64_t>>;
    [[nodiscard]] const DeletedFileSet& deleted_files() const { return deleted_files_; }
    [[nodiscard]] const std::vector<std::pair<int, FileMetaData>>& new_files() const {
        return new_files_;
    }

private:
    friend class VersionSet;

    std::string comparator_;
    uint64_t log_number_{0};
    uint64_t prev_log_number_{0};
    uint64_t next_file_number_{0};
    SequenceNumber last_sequence_{0};
    bool has_comparator_{false};
    bool has_log_number_{false};
    bool has_prev_log_number_{false};
    bool has_next_file_number_{false};
    bool has_last_sequence_{false};

    DeletedFileSet deleted_files_;
    std::vector<std::pair<int, FileMetaData>> new_files_;
};

} // namespace lsmforge
