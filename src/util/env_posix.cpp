#include "util/env.h"
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace lsmforge {

namespace {

Status IOError(const std::string& context, int err_number) {
    return Status::IOError(context, strerror(err_number));
}

class PosixSequentialFile : public SequentialFile {
public:
    PosixSequentialFile(std::string fname, int fd)
        : filename_(std::move(fname)), fd_(fd) {}

    ~PosixSequentialFile() override {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    Status Read(size_t n, Slice* result, char* scratch) override {
        Status s;
        while (true) {
            ssize_t r = ::read(fd_, scratch, n);
            if (r < 0) {
                if (errno == EINTR) {
                    continue; // Retry on interrupt
                }
                s = IOError(filename_, errno);
                break;
            }
            *result = Slice(scratch, static_cast<size_t>(r));
            break;
        }
        return s;
    }

    Status Skip(uint64_t n) override {
        if (::lseek(fd_, static_cast<off_t>(n), SEEK_CUR) == static_cast<off_t>(-1)) {
            return IOError(filename_, errno);
        }
        return Status::OK();
    }

private:
    std::string filename_;
    int fd_;
};

class PosixRandomAccessFile : public RandomAccessFile {
public:
    PosixRandomAccessFile(std::string fname, int fd)
        : filename_(std::move(fname)), fd_(fd) {}

    ~PosixRandomAccessFile() override {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const override {
        Status s;
        ssize_t r = -1;
        while (true) {
            r = ::pread(fd_, scratch, n, static_cast<off_t>(offset));
            if (r < 0) {
                if (errno == EINTR) {
                    continue;
                }
                s = IOError(filename_, errno);
                break;
            }
            *result = Slice(scratch, static_cast<size_t>(r));
            break;
        }
        return s;
    }

private:
    std::string filename_;
    int fd_;
};

class PosixWritableFile : public WritableFile {
public:
    PosixWritableFile(std::string fname, int fd)
        : filename_(std::move(fname)), fd_(fd), pos_(0) {}

    ~PosixWritableFile() override {
        if (fd_ >= 0) {
            Close();
        }
    }

    Status Append(const Slice& data) override {
        const char* src = data.data();
        size_t left = data.size();
        while (left > 0) {
            ssize_t done = ::write(fd_, src, left);
            if (done < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return IOError(filename_, errno);
            }
            src += done;
            left -= static_cast<size_t>(done);
            pos_ += static_cast<size_t>(done);
        }
        return Status::OK();
    }

    Status Close() override {
        Status s = Flush();
        if (fd_ >= 0) {
            if (::close(fd_) < 0 && s.ok()) {
                s = IOError(filename_, errno);
            }
            fd_ = -1;
        }
        return s;
    }

    Status Flush() override {
        return Status::OK();
    }

    Status Sync() override {
#if defined(__APPLE__)
        // On macOS / Darwin, fcntl with F_FULLFSYNC instructs the storage controller
        // to flush all caches through to physical non-volatile media.
        if (::fcntl(fd_, F_FULLFSYNC) < 0) {
            // Fall back to fsync if F_FULLFSYNC fails (e.g. on unsupported filesystems)
            if (::fsync(fd_) < 0) {
                return IOError(filename_, errno);
            }
        }
#elif defined(_POSIX_SYNCHRONIZED_IO) && (_POSIX_SYNCHRONIZED_IO > 0)
        if (::fdatasync(fd_) < 0) {
            return IOError(filename_, errno);
        }
#else
        if (::fsync(fd_) < 0) {
            return IOError(filename_, errno);
        }
#endif
        return Status::OK();
    }

private:
    std::string filename_;
    int fd_;
    size_t pos_;
};

class PosixFileLock : public FileLock {
public:
    PosixFileLock(int fd, std::string fname) : fd_(fd), filename_(std::move(fname)) {}
    ~PosixFileLock() override {
        if (fd_ >= 0) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
        }
    }
    int fd_;
    std::string filename_;
};

class PosixEnv : public Env {
public:
    PosixEnv() = default;
    ~PosixEnv() override = default;

    Status NewSequentialFile(const std::string& fname,
                             std::unique_ptr<SequentialFile>* result) override {
        result->reset();
        int fd = ::open(fname.c_str(), O_RDONLY);
        if (fd < 0) {
            return IOError(fname, errno);
        }
        result->reset(new PosixSequentialFile(fname, fd));
        return Status::OK();
    }

    Status NewRandomAccessFile(const std::string& fname,
                               std::unique_ptr<RandomAccessFile>* result) override {
        result->reset();
        int fd = ::open(fname.c_str(), O_RDONLY);
        if (fd < 0) {
            return IOError(fname, errno);
        }
        result->reset(new PosixRandomAccessFile(fname, fd));
        return Status::OK();
    }

    Status NewWritableFile(const std::string& fname,
                           std::unique_ptr<WritableFile>* result) override {
        result->reset();
        int fd = ::open(fname.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            return IOError(fname, errno);
        }
        result->reset(new PosixWritableFile(fname, fd));
        return Status::OK();
    }

    Status NewAppendableFile(const std::string& fname,
                             std::unique_ptr<WritableFile>* result) override {
        result->reset();
        int fd = ::open(fname.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            return IOError(fname, errno);
        }
        result->reset(new PosixWritableFile(fname, fd));
        return Status::OK();
    }

    bool FileExists(const std::string& fname) override {
        return ::access(fname.c_str(), F_OK) == 0;
    }

    Status GetChildren(const std::string& dir, std::vector<std::string>* result) override {
        result->clear();
        DIR* d = ::opendir(dir.c_str());
        if (d == nullptr) {
            return IOError(dir, errno);
        }
        struct dirent* entry;
        while ((entry = ::readdir(d)) != nullptr) {
            if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0) {
                result->emplace_back(entry->d_name);
            }
        }
        ::closedir(d);
        return Status::OK();
    }

    Status RemoveFile(const std::string& fname) override {
        if (::unlink(fname.c_str()) != 0) {
            return IOError(fname, errno);
        }
        return Status::OK();
    }

    Status CreateDir(const std::string& dirname) override {
        if (::mkdir(dirname.c_str(), 0755) != 0) {
            return IOError(dirname, errno);
        }
        return Status::OK();
    }

    Status CreateDirIfMissing(const std::string& dirname) override {
        if (CreateDir(dirname).ok()) {
            return Status::OK();
        }
        if (FileExists(dirname)) {
            return Status::OK();
        }
        return IOError(dirname, errno);
    }

    Status RemoveDir(const std::string& dirname) override {
        if (::rmdir(dirname.c_str()) != 0) {
            return IOError(dirname, errno);
        }
        return Status::OK();
    }

    Status GetFileSize(const std::string& fname, uint64_t* file_size) override {
        struct stat sbuf;
        if (::stat(fname.c_str(), &sbuf) != 0) {
            *file_size = 0;
            return IOError(fname, errno);
        }
        *file_size = static_cast<uint64_t>(sbuf.st_size);
        return Status::OK();
    }

    Status RenameFile(const std::string& src, const std::string& target) override {
        if (::rename(src.c_str(), target.c_str()) != 0) {
            return IOError(src, errno);
        }
        return Status::OK();
    }

    Status LockFile(const std::string& fname, FileLock** lock) override {
        *lock = nullptr;
        int fd = ::open(fname.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd < 0) {
            return IOError(fname, errno);
        }
        if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
            ::close(fd);
            return Status::IOError("LockFile failed: already locked by another process", fname);
        }
        *lock = new PosixFileLock(fd, fname);
        return Status::OK();
    }

    Status UnlockFile(FileLock* lock) override {
        PosixFileLock* my_lock = static_cast<PosixFileLock*>(lock);
        delete my_lock;
        return Status::OK();
    }

    uint64_t NowMicros() override {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }

    void SleepMicros(uint64_t micros) override {
        std::this_thread::sleep_for(std::chrono::microseconds(micros));
    }
};

} // namespace

Env* Env::Default() {
    static PosixEnv default_env;
    return &default_env;
}

Status WriteStringToFileSync(Env* env, const Slice& data, const std::string& fname) {
    std::unique_ptr<WritableFile> file;
    Status s = env->NewWritableFile(fname, &file);
    if (!s.ok()) {
        return s;
    }
    s = file->Append(data);
    if (s.ok()) {
        s = file->Sync();
    }
    if (s.ok()) {
        s = file->Close();
    }
    return s;
}

Status ReadFileToString(Env* env, const std::string& fname, std::string* data) {
    data->clear();
    std::unique_ptr<SequentialFile> file;
    Status s = env->NewSequentialFile(fname, &file);
    if (!s.ok()) {
        return s;
    }
    static const size_t kBufferSize = 8192;
    char buffer[kBufferSize];
    while (true) {
        Slice fragment;
        s = file->Read(kBufferSize, &fragment, buffer);
        if (!s.ok()) {
            break;
        }
        if (fragment.empty()) {
            break;
        }
        data->append(fragment.data(), fragment.size());
    }
    return s;
}

} // namespace lsmforge
