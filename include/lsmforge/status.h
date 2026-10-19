#pragma once

#include "lsmforge/slice.h"
#include <string>

namespace lsmforge {

class Status {
public:
    enum Code {
        kOk = 0,
        kNotFound = 1,
        kCorruption = 2,
        kIOError = 3,
        kInvalidArgument = 4,
        kBusy = 5,
        kUnsupportedOperation = 6
    };

    Status() noexcept : state_(nullptr) {}
    ~Status() { delete[] state_; }

    Status(const Status& rhs);
    Status& operator=(const Status& rhs);

    Status(Status&& rhs) noexcept : state_(rhs.state_) { rhs.state_ = nullptr; }
    Status& operator=(Status&& rhs) noexcept {
        if (this != &rhs) {
            delete[] state_;
            state_ = rhs.state_;
            rhs.state_ = nullptr;
        }
        return *this;
    }

    static Status OK() { return Status(); }
    static Status NotFound(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kNotFound, msg, msg2);
    }
    static Status Corruption(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kCorruption, msg, msg2);
    }
    static Status IOError(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kIOError, msg, msg2);
    }
    static Status InvalidArgument(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kInvalidArgument, msg, msg2);
    }
    static Status Busy(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kBusy, msg, msg2);
    }
    static Status UnsupportedOperation(const Slice& msg, const Slice& msg2 = Slice()) {
        return Status(kUnsupportedOperation, msg, msg2);
    }

    [[nodiscard]] bool ok() const noexcept { return state_ == nullptr; }
    [[nodiscard]] bool IsNotFound() const noexcept { return code() == kNotFound; }
    [[nodiscard]] bool IsCorruption() const noexcept { return code() == kCorruption; }
    [[nodiscard]] bool IsIOError() const noexcept { return code() == kIOError; }
    [[nodiscard]] bool IsInvalidArgument() const noexcept { return code() == kInvalidArgument; }
    [[nodiscard]] bool IsBusy() const noexcept { return code() == kBusy; }
    [[nodiscard]] bool IsUnsupportedOperation() const noexcept { return code() == kUnsupportedOperation; }

    [[nodiscard]] Code code() const noexcept {
        return (state_ == nullptr) ? kOk : static_cast<Code>(state_[4]);
    }

    [[nodiscard]] std::string ToString() const;

private:
    // state_ structure:
    // state_[0..3] == length of message
    // state_[4]    == code
    // state_[5..]  == message
    const char* state_;

    Status(Code code, const Slice& msg, const Slice& msg2);
    static const char* CopyState(const char* s);
};

} // namespace lsmforge
