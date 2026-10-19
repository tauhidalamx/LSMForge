#include "table/table_reader.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include <cassert>

namespace lsmforge {

struct TableReader::Rep {
    Options options;
    Status status;
    std::unique_ptr<RandomAccessFile> file;
    BlockHandle metaindex_handle;
    std::unique_ptr<Block> index_block;
    std::string filter_data;
    std::unique_ptr<BloomFilterPolicy> filter_policy;
};

TableReader::TableReader(std::unique_ptr<Rep> rep) : rep_(std::move(rep)) {}

TableReader::~TableReader() = default;

Status TableReader::Open(const Options& options,
                         std::unique_ptr<RandomAccessFile> file,
                         uint64_t size,
                         std::unique_ptr<TableReader>* table) {
    table->reset();
    if (size < Footer::kEncodedLength) {
        return Status::Corruption("file is too short to be an sstable");
    }

    // 1. Read Footer
    char footer_space[Footer::kEncodedLength];
    Slice footer_input;
    Status s = file->Read(size - Footer::kEncodedLength, Footer::kEncodedLength,
                          &footer_input, footer_space);
    if (!s.ok()) return s;

    Footer footer;
    s = footer.DecodeFrom(&footer_input);
    if (!s.ok()) return s;

    // 2. Read Index Block
    ReadOptions opt;
    if (options.paranoid_checks) {
        opt.verify_checksums = true;
    }
    BlockContents index_block_contents;
    s = ReadBlock(file.get(), opt, footer.index_handle(), &index_block_contents);
    if (!s.ok()) return s;

    auto rep = std::make_unique<Rep>();
    rep->options = options;
    rep->file = std::move(file);
    rep->metaindex_handle = footer.metaindex_handle();
    rep->index_block = std::make_unique<Block>(std::move(index_block_contents));

    // 3. Read MetaIndex Block & Bloom Filter
    BlockContents meta_contents;
    s = ReadBlock(rep->file.get(), opt, footer.metaindex_handle(), &meta_contents);
    if (s.ok()) {
        Block meta_block(std::move(meta_contents));
        std::unique_ptr<Iterator> iter(meta_block.NewIterator(BytewiseComparator()));
        iter->SeekToFirst();
        while (iter->Valid()) {
            Slice key = iter->key();
            if (key.starts_with("filter.")) {
                Slice handle_slice = iter->value();
                BlockHandle filter_handle;
                if (filter_handle.DecodeFrom(&handle_slice).ok()) {
                    BlockContents filter_contents;
                    if (ReadBlock(rep->file.get(), opt, filter_handle, &filter_contents).ok()) {
                        rep->filter_data = filter_contents.data.ToString();
                        rep->filter_policy = std::make_unique<BloomFilterPolicy>(options.bloom_bits_per_key);
                    }
                }
                break;
            }
            iter->Next();
        }
    }

    *table = std::unique_ptr<TableReader>(new TableReader(std::move(rep)));
    return Status::OK();
}

bool TableReader::KeyMayMatch(const Slice& user_key) const {
    if (rep_->filter_policy && !rep_->filter_data.empty()) {
        return rep_->filter_policy->KeyMayMatch(user_key, Slice(rep_->filter_data));
    }
    return true; // Conservative fallback: might match
}

Iterator* TableReader::BlockReader(void* arg, const ReadOptions& options, const Slice& index_value) {
    TableReader* table = reinterpret_cast<TableReader*>(arg);
    Slice handle_slice = index_value;
    BlockHandle handle;
    Status s = handle.DecodeFrom(&handle_slice);
    if (!s.ok()) {
        return NewErrorIterator(s);
    }
    BlockContents contents;
    s = ReadBlock(table->rep_->file.get(), options, handle, &contents);
    if (!s.ok()) {
        return NewErrorIterator(s);
    }
    Block* block = new Block(std::move(contents));
    Iterator* iter = block->NewIterator(table->rep_->options.comparator);
    iter->RegisterCleanup([](void* a1, void*) {
        delete static_cast<Block*>(a1);
    }, block, nullptr);
    return iter;
}

Iterator* TableReader::NewIterator(const ReadOptions& options) {
    return NewTwoLevelIterator(
        rep_->index_block->NewIterator(rep_->options.comparator),
        &TableReader::BlockReader,
        this,
        options);
}

} // namespace lsmforge
