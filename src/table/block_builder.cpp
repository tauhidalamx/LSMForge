#include "table/block_builder.h"
#include "util/coding.h"
#include <algorithm>
#include <cassert>

namespace lsmforge {

BlockBuilder::BlockBuilder(const Options* options)
    : options_(options),
      restarts_(),
      counter_(0),
      finished_(false) {
    assert(options->block_restart_interval >= 1);
    restarts_.push_back(0); // First restart point is always at offset 0
}

void BlockBuilder::Reset() {
    buffer_.clear();
    restarts_.clear();
    restarts_.push_back(0); // First restart point is at offset 0
    counter_ = 0;
    finished_ = false;
    last_key_.clear();
}

size_t BlockBuilder::CurrentSizeEstimate() const {
    return (buffer_.size() +                      // Raw data buffer
            restarts_.size() * sizeof(uint32_t) + // Restart array
            sizeof(uint32_t));                    // Restart array length
}

Slice BlockBuilder::Finish() {
    // Append restart array
    for (size_t i = 0; i < restarts_.size(); i++) {
        PutFixed32(&buffer_, restarts_[i]);
    }
    PutFixed32(&buffer_, static_cast<uint32_t>(restarts_.size()));
    finished_ = true;
    return Slice(buffer_);
}

void BlockBuilder::Add(const Slice& key, const Slice& value) {
    Slice last_key_piece(last_key_);
    assert(!finished_);
    assert(counter_ <= options_->block_restart_interval);
    assert(buffer_.empty() // First key in block
           || options_->comparator->Compare(key, last_key_piece) > 0);

    size_t shared = 0;
    if (counter_ < options_->block_restart_interval) {
        // Calculate common shared prefix
        const size_t min_length = std::min(last_key_piece.size(), key.size());
        while ((shared < min_length) && (last_key_piece[shared] == key[shared])) {
            shared++;
        }
    } else {
        // Restart compression
        restarts_.push_back(static_cast<uint32_t>(buffer_.size()));
        counter_ = 0;
    }
    const size_t non_shared = key.size() - shared;

    // Add "<shared><non_shared><value_size>" to buffer_
    PutVarint32(&buffer_, static_cast<uint32_t>(shared));
    PutVarint32(&buffer_, static_cast<uint32_t>(non_shared));
    PutVarint32(&buffer_, static_cast<uint32_t>(value.size()));

    // Add string delta followed by value bytes
    buffer_.append(key.data() + shared, non_shared);
    buffer_.append(value.data(), value.size());

    // Update state
    last_key_.resize(shared);
    last_key_.append(key.data() + shared, non_shared);
    assert(Slice(last_key_) == key);
    counter_++;
}

} // namespace lsmforge
