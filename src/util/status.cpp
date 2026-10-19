#include "lsmforge/status.h"
#include <cstdint>
#include <cstring>

namespace lsmforge {

const char* Status::CopyState(const char* s) {
    uint32_t size;
    std::memcpy(&size, s, sizeof(size));
    char* result = new char[size + 5];
    std::memcpy(result, s, size + 5);
    return result;
}

Status::Status(const Status& rhs) {
    state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
}

Status& Status::operator=(const Status& rhs) {
    if (this != &rhs) {
        delete[] state_;
        state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
    }
    return *this;
}

Status::Status(Code code, const Slice& msg, const Slice& msg2) {
    const uint32_t len1 = static_cast<uint32_t>(msg.size());
    const uint32_t len2 = static_cast<uint32_t>(msg2.size());
    const uint32_t size = len1 + (len2 > 0 ? (2 + len2) : 0);
    char* result = new char[size + 5];
    std::memcpy(result, &size, sizeof(size));
    result[4] = static_cast<char>(code);
    std::memcpy(result + 5, msg.data(), len1);
    if (len2 > 0) {
        result[5 + len1] = ':';
        result[6 + len1] = ' ';
        std::memcpy(result + 7 + len1, msg2.data(), len2);
    }
    state_ = result;
}

std::string Status::ToString() const {
    if (state_ == nullptr) {
        return "OK";
    }
    const char* type = nullptr;
    switch (code()) {
        case kOk: type = "OK"; break;
        case kNotFound: type = "NotFound: "; break;
        case kCorruption: type = "Corruption: "; break;
        case kIOError: type = "IOError: "; break;
        case kInvalidArgument: type = "InvalidArgument: "; break;
        case kBusy: type = "Busy: "; break;
        case kUnsupportedOperation: type = "UnsupportedOperation: "; break;
    }
    std::string result(type);
    uint32_t length;
    std::memcpy(&length, state_, sizeof(length));
    result.append(state_ + 5, length);
    return result;
}

} // namespace lsmforge
