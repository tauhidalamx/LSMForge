#include "db/version_edit.h"
#include "db/filename.h"
#include "util/coding.h"
#include <sstream>

namespace lsmforge {

namespace {

enum Tag {
    kComparator     = 1,
    kLogNumber      = 2,
    kNextFileNumber = 3,
    kLastSequence   = 4,
    kCompactPointer = 5,
    kDeletedFile    = 6,
    kNewFile        = 7,
    kPrevLogNumber  = 9
};

} // namespace

void VersionEdit::Clear() {
    comparator_.clear();
    log_number_ = 0;
    prev_log_number_ = 0;
    next_file_number_ = 0;
    last_sequence_ = 0;
    has_comparator_ = false;
    has_log_number_ = false;
    has_prev_log_number_ = false;
    has_next_file_number_ = false;
    has_last_sequence_ = false;
    deleted_files_.clear();
    new_files_.clear();
}

void VersionEdit::EncodeTo(std::string* dst) const {
    if (has_comparator_) {
        PutVarint32(dst, kComparator);
        PutLengthPrefixedSlice(dst, comparator_);
    }
    if (has_log_number_) {
        PutVarint32(dst, kLogNumber);
        PutVarint64(dst, log_number_);
    }
    if (has_prev_log_number_) {
        PutVarint32(dst, kPrevLogNumber);
        PutVarint64(dst, prev_log_number_);
    }
    if (has_next_file_number_) {
        PutVarint32(dst, kNextFileNumber);
        PutVarint64(dst, next_file_number_);
    }
    if (has_last_sequence_) {
        PutVarint32(dst, kLastSequence);
        PutVarint64(dst, last_sequence_);
    }

    for (const auto& deleted : deleted_files_) {
        PutVarint32(dst, kDeletedFile);
        PutVarint32(dst, deleted.first);
        PutVarint64(dst, deleted.second);
    }

    for (const auto& new_file : new_files_) {
        const FileMetaData& f = new_file.second;
        PutVarint32(dst, kNewFile);
        PutVarint32(dst, new_file.first);
        PutVarint64(dst, f.number);
        PutVarint64(dst, f.file_size);
        PutLengthPrefixedSlice(dst, f.smallest);
        PutLengthPrefixedSlice(dst, f.largest);
    }
}

Status VersionEdit::DecodeFrom(const Slice& src) {
    Clear();
    Slice input = src;
    uint32_t tag;

    while (GetVarint32(&input, &tag)) {
        switch (tag) {
            case kComparator: {
                Slice str;
                if (GetLengthPrefixedSlice(&input, &str)) {
                    comparator_ = str.ToString();
                    has_comparator_ = true;
                } else {
                    return Status::Corruption("VersionEdit: comparator");
                }
                break;
            }
            case kLogNumber: {
                if (GetVarint64(&input, &log_number_)) {
                    has_log_number_ = true;
                } else {
                    return Status::Corruption("VersionEdit: log_number");
                }
                break;
            }
            case kPrevLogNumber: {
                if (GetVarint64(&input, &prev_log_number_)) {
                    has_prev_log_number_ = true;
                } else {
                    return Status::Corruption("VersionEdit: prev_log_number");
                }
                break;
            }
            case kNextFileNumber: {
                if (GetVarint64(&input, &next_file_number_)) {
                    has_next_file_number_ = true;
                } else {
                    return Status::Corruption("VersionEdit: next_file_number");
                }
                break;
            }
            case kLastSequence: {
                uint64_t seq;
                if (GetVarint64(&input, &seq)) {
                    last_sequence_ = seq;
                    has_last_sequence_ = true;
                } else {
                    return Status::Corruption("VersionEdit: last_sequence");
                }
                break;
            }
            case kDeletedFile: {
                uint32_t level;
                uint64_t number;
                if (GetVarint32(&input, &level) && GetVarint64(&input, &number)) {
                    deleted_files_.insert(std::make_pair(static_cast<int>(level), number));
                } else {
                    return Status::Corruption("VersionEdit: deleted_file");
                }
                break;
            }
            case kNewFile: {
                uint32_t level;
                uint64_t number;
                uint64_t file_size;
                Slice smallest, largest;
                if (GetVarint32(&input, &level) &&
                    GetVarint64(&input, &number) &&
                    GetVarint64(&input, &file_size) &&
                    GetLengthPrefixedSlice(&input, &smallest) &&
                    GetLengthPrefixedSlice(&input, &largest)) {
                    FileMetaData f;
                    f.number = number;
                    f.file_size = file_size;
                    f.smallest = smallest.ToString();
                    f.largest = largest.ToString();
                    new_files_.emplace_back(static_cast<int>(level), f);
                } else {
                    return Status::Corruption("VersionEdit: new_file");
                }
                break;
            }
            default:
                return Status::Corruption("VersionEdit: unknown tag");
        }
    }
    return Status::OK();
}

bool ParseFileName(const std::string& filename, uint64_t* number, FileType* type) {
    Slice rest(filename);
    if (rest == "CURRENT") {
        *number = 0;
        *type = kCurrentFile;
    } else if (rest == "LOCK") {
        *number = 0;
        *type = kDBLockFile;
    } else if (rest.starts_with("MANIFEST-")) {
        rest.remove_prefix(9);
        uint64_t num;
        Slice input = rest;
        char* end;
        num = strtoull(input.data(), &end, 10);
        if (end == input.data()) return false;
        *number = num;
        *type = kDescriptorFile;
    } else {
        // Find last dot
        size_t dot = filename.find_last_of('.');
        if (dot == std::string::npos) return false;
        std::string ext = filename.substr(dot);
        std::string base = filename.substr(0, dot);
        size_t slash = base.find_last_of('/');
        if (slash != std::string::npos) {
            base = base.substr(slash + 1);
        }
        char* end;
        uint64_t num = strtoull(base.c_str(), &end, 10);
        if (end == base.c_str()) return false;
        *number = num;
        if (ext == ".wal") {
            *type = kLogFile;
        } else if (ext == ".sst") {
            *type = kTableFile;
        } else if (ext == ".tmp") {
            *type = kTempFile;
        } else {
            return false;
        }
    }
    return true;
}

Status SetCurrentFile(Env* env, const std::string& dbname, uint64_t descriptor_number) {
    // Atomically write CURRENT file by writing to tmp and renaming
    char manifest_name[100];
    std::snprintf(manifest_name, sizeof(manifest_name), "MANIFEST-%06llu\n",
                  static_cast<unsigned long long>(descriptor_number));
    std::string contents(manifest_name);
    std::string tmp_name = TempFileName(dbname, descriptor_number);
    Status s = WriteStringToFileSync(env, contents, tmp_name);
    if (s.ok()) {
        s = env->RenameFile(tmp_name, CurrentFileName(dbname));
    }
    if (!s.ok()) {
        env->RemoveFile(tmp_name);
    }
    return s;
}

} // namespace lsmforge
