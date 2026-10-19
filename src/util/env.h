#pragma once

#include "lsmforge/slice.h"
#include "lsmforge/status.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lsmforge {

class FileLock {
public:
    FileLock() = default;
    virtual ~FileLock() = default;
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
};

// Sequential read interface
class SequentialFile {
public:
    SequentialFile() = default;
    virtual ~SequentialFile() = default;
    SequentialFile(const SequentialFile&) = delete;
    SequentialFile& operator=(const SequentialFile&) = delete;

    virtual Status Read(size_t n, Slice* result, char* scratch) = 0;
    virtual Status Skip(uint64_t n) = 0;
};

// Random access read interface (thread-safe, multiple concurrent readers)
class RandomAccessFile {
public:
    RandomAccessFile() = default;
    virtual ~RandomAccessFile() = default;
    RandomAccessFile(const RandomAccessFile&) = delete;
    RandomAccessFile& operator=(const RandomAccessFile&) = delete;

    virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const = 0;
};

// Writable file interface
class WritableFile {
public:
    WritableFile() = default;
    virtual ~WritableFile() = default;
    WritableFile(const WritableFile&) = delete;
    WritableFile& operator=(const WritableFile&) = delete;

    virtual Status Append(const Slice& data) = 0;
    virtual Status Close() = 0;
    virtual Status Flush() = 0;
    virtual Status Sync() = 0;
};

// Env interface for all OS and filesystem interactions
class Env {
public:
    Env() = default;
    virtual ~Env() = default;
    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;

    static Env* Default();

    virtual Status NewSequentialFile(const std::string& fname,
                                     std::unique_ptr<SequentialFile>* result) = 0;

    virtual Status NewRandomAccessFile(const std::string& fname,
                                       std::unique_ptr<RandomAccessFile>* result) = 0;

    virtual Status NewWritableFile(const std::string& fname,
                                   std::unique_ptr<WritableFile>* result) = 0;

    virtual Status NewAppendableFile(const std::string& fname,
                                     std::unique_ptr<WritableFile>* result) = 0;

    virtual bool FileExists(const std::string& fname) = 0;

    virtual Status GetChildren(const std::string& dir,
                               std::vector<std::string>* result) = 0;

    virtual Status RemoveFile(const std::string& fname) = 0;

    virtual Status CreateDir(const std::string& dirname) = 0;

    virtual Status CreateDirIfMissing(const std::string& dirname) = 0;

    virtual Status RemoveDir(const std::string& dirname) = 0;

    virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) = 0;

    virtual Status RenameFile(const std::string& src, const std::string& target) = 0;

    virtual Status LockFile(const std::string& fname, FileLock** lock) = 0;

    virtual Status UnlockFile(FileLock* lock) = 0;

    virtual uint64_t NowMicros() = 0;

    virtual void SleepMicros(uint64_t micros) = 0;
};

// Write helper for atomic file updates (write to tmp and rename)
Status WriteStringToFileSync(Env* env, const Slice& data, const std::string& fname);
Status ReadFileToString(Env* env, const std::string& fname, std::string* data);

} // namespace lsmforge
