#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

namespace lsmforge {

/// Slice is a simple wrapper around a pointer to a byte sequence and its length.
/// It does NOT own the underlying memory. The caller must ensure that the memory
/// pointed to remains valid for the lifetime of the Slice.
class Slice {
public:
    constexpr Slice() noexcept : data_(""), size_(0) {}
    
    constexpr Slice(const char* d, size_t n) noexcept : data_(d), size_(n) {}

    Slice(const std::string& s) noexcept : data_(s.data()), size_(s.size()) {}

    constexpr Slice(const char* s) noexcept 
        : data_(s ? s : ""), size_(s ? std::char_traits<char>::length(s) : 0) {}

    constexpr Slice(std::string_view sv) noexcept : data_(sv.data()), size_(sv.size()) {}

    [[nodiscard]] constexpr const char* data() const noexcept { return data_; }
    [[nodiscard]] constexpr size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr char operator[](size_t n) const noexcept {
        assert(n < size());
        return data_[n];
    }

    void clear() noexcept {
        data_ = "";
        size_ = 0;
    }

    void remove_prefix(size_t n) noexcept {
        assert(n <= size());
        data_ += n;
        size_ -= n;
    }

    [[nodiscard]] std::string ToString() const {
        return std::string(data_, size_);
    }

    [[nodiscard]] constexpr std::string_view ToStringView() const noexcept {
        return std::string_view(data_, size_);
    }

    [[nodiscard]] int compare(const Slice& b) const noexcept {
        const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
        int r = std::memcmp(data_, b.data_, min_len);
        if (r == 0) {
            if (size_ < b.size_) return -1;
            if (size_ > b.size_) return 1;
            return 0;
        }
        return r;
    }

    [[nodiscard]] bool starts_with(const Slice& x) const noexcept {
        return (size_ >= x.size_) && (std::memcmp(data_, x.data_, x.size_) == 0);
    }

private:
    const char* data_;
    size_t size_;
};

inline bool operator==(const Slice& x, const Slice& y) noexcept {
    return (x.size() == y.size()) && (std::memcmp(x.data(), y.data(), x.size()) == 0);
}

inline bool operator!=(const Slice& x, const Slice& y) noexcept {
    return !(x == y);
}

inline bool operator<(const Slice& x, const Slice& y) noexcept {
    return x.compare(y) < 0;
}

inline bool operator<=(const Slice& x, const Slice& y) noexcept {
    return x.compare(y) <= 0;
}

inline bool operator>(const Slice& x, const Slice& y) noexcept {
    return x.compare(y) > 0;
}

inline bool operator>=(const Slice& x, const Slice& y) noexcept {
    return x.compare(y) >= 0;
}

} // namespace lsmforge
