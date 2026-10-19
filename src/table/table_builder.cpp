#include "table/table_builder.h"
#include "util/coding.h"
#include "util/crc32.h"
#include <cassert>

namespace lsmforge {

TableBuilder::TableBuilder(const Options& options, WritableFile* file)
    : options_(options),
      file_(file),
      data_block_(&options_),
      index_block_(&options_) {
    if (options.bloom_bits_per_key > 0) {
        filter_policy_.reset(new BloomFilterPolicy(options.bloom_bits_per_key));
    }
}

TableBuilder::~TableBuilder() {
    assert(closed_);
}

void TableBuilder::Add(const Slice& key, const Slice& value) {
    assert(!closed_);
    if (!status_.ok()) return;

    if (num_entries_ > 0) {
        assert(options_.comparator->Compare(key, Slice(last_key_)) > 0);
    } else {
        smallest_key_ = key.ToString();
    }

    if (pending_index_entry_) {
        assert(data_block_.empty());
        // Add index entry for previous data block
        std::string handle_encoding;
        pending_handle_.EncodeTo(&handle_encoding);
        index_block_.Add(last_key_, handle_encoding);
        pending_index_entry_ = false;
    }

    if (filter_policy_) {
        // Index user key into bloom filter
        Slice user_key = (key.size() >= 8) ? ExtractUserKey(key) : key;
        filter_keys_.push_back(user_key.ToString());
    }

    last_key_.assign(key.data(), key.size());
    largest_key_.assign(key.data(), key.size());
    num_entries_++;
    data_block_.Add(key, value);

    const size_t estimated_block_size = data_block_.CurrentSizeEstimate();
    if (estimated_block_size >= options_.block_size) {
        Flush();
    }
}

void TableBuilder::Flush() {
    assert(!closed_);
    if (!status_.ok() || data_block_.empty()) return;

    assert(!pending_index_entry_);
    WriteBlock(&data_block_, &pending_handle_);
    if (status_.ok()) {
        pending_index_entry_ = true;
        status_ = file_->Flush();
    }
}

void TableBuilder::WriteBlock(BlockBuilder* block, BlockHandle* handle) {
    assert(status_.ok());
    Slice raw = block->Finish();

    handle->set_offset(offset_);
    handle->set_size(raw.size());
    status_ = file_->Append(raw);

    if (status_.ok()) {
        // Trailer: 1 byte type (0 = no compression) + 4 bytes CRC
        char trailer[kBlockTrailerSize];
        trailer[0] = 0; // kNoCompression
        uint32_t crc = crc32::Extend(0, raw.data(), raw.size());
        crc = crc32::Extend(crc, trailer, 1);
        crc = crc32::Mask(crc);
        EncodeFixed32(trailer + 1, crc);
        status_ = file_->Append(Slice(trailer, kBlockTrailerSize));
        if (status_.ok()) {
            offset_ += raw.size() + kBlockTrailerSize;
        }
    }
    block->Reset();
}

Status TableBuilder::Finish() {
    assert(!closed_);
    Flush();
    closed_ = true;

    BlockHandle filter_block_handle;
    BlockHandle metaindex_block_handle;
    BlockHandle index_block_handle;

    // 1. Write Bloom Filter Block
    if (status_.ok() && filter_policy_ && !filter_keys_.empty()) {
        std::vector<Slice> key_slices;
        key_slices.reserve(filter_keys_.size());
        for (const auto& k : filter_keys_) {
            key_slices.emplace_back(k);
        }
        std::string filter_data;
        filter_policy_->CreateFilter(key_slices.data(), static_cast<int>(key_slices.size()),
                                     &filter_data);

        // Write raw filter block
        filter_block_handle.set_offset(offset_);
        filter_block_handle.set_size(filter_data.size());
        status_ = file_->Append(Slice(filter_data));
        if (status_.ok()) {
            char trailer[kBlockTrailerSize];
            trailer[0] = 0;
            uint32_t crc = crc32::Extend(0, filter_data.data(), filter_data.size());
            crc = crc32::Extend(crc, trailer, 1);
            crc = crc32::Mask(crc);
            EncodeFixed32(trailer + 1, crc);
            status_ = file_->Append(Slice(trailer, kBlockTrailerSize));
            if (status_.ok()) {
                offset_ += filter_data.size() + kBlockTrailerSize;
            }
        }
    }

    // 2. Write MetaIndex Block
    if (status_.ok()) {
        BlockBuilder meta_index_block(&options_);
        if (filter_policy_ && !filter_keys_.empty()) {
            std::string key = "filter.";
            key.append(filter_policy_->Name());
            std::string handle_encoding;
            filter_block_handle.EncodeTo(&handle_encoding);
            meta_index_block.Add(key, handle_encoding);
        }
        WriteBlock(&meta_index_block, &metaindex_block_handle);
    }

    // 3. Write Index Block
    if (status_.ok()) {
        if (pending_index_entry_) {
            std::string handle_encoding;
            pending_handle_.EncodeTo(&handle_encoding);
            index_block_.Add(last_key_, handle_encoding);
            pending_index_entry_ = false;
        }
        WriteBlock(&index_block_, &index_block_handle);
    }

    // 4. Write Footer
    if (status_.ok()) {
        Footer footer;
        footer.set_metaindex_handle(metaindex_block_handle);
        footer.set_index_handle(index_block_handle);
        std::string footer_encoding;
        footer.EncodeTo(&footer_encoding);
        status_ = file_->Append(footer_encoding);
        if (status_.ok()) {
            offset_ += footer_encoding.size();
        }
    }

    return status_;
}

void TableBuilder::Abandon() {
    assert(!closed_);
    closed_ = true;
}

} // namespace lsmforge
