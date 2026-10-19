#include "table/table_builder.h"
#include "table/table_reader.h"
#include <gtest/gtest.h>

namespace lsmforge {

class MemoryWritableFile : public WritableFile {
public:
    std::string data;
    Status Append(const Slice& d) override {
        data.append(d.data(), d.size());
        return Status::OK();
    }
    Status Close() override { return Status::OK(); }
    Status Flush() override { return Status::OK(); }
    Status Sync() override { return Status::OK(); }
};

class MemoryRandomAccessFile : public RandomAccessFile {
public:
    explicit MemoryRandomAccessFile(std::string d) : data_(std::move(d)) {}
    Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const override {
        if (offset >= data_.size()) {
            *result = Slice();
            return Status::OK();
        }
        size_t available = data_.size() - offset;
        size_t to_read = std::min(n, available);
        std::memcpy(scratch, data_.data() + offset, to_read);
        *result = Slice(scratch, to_read);
        return Status::OK();
    }
private:
    std::string data_;
};

TEST(TableTest, BuildAndReadSSTable) {
    Options options;
    options.bloom_bits_per_key = 10;
    options.block_size = 512; // Small block size to create multiple blocks

    MemoryWritableFile outfile;
    TableBuilder builder(options, &outfile);

    // Insert 1000 keys
    for (int i = 0; i < 1000; ++i) {
        char kbuf[32];
        std::snprintf(kbuf, sizeof(kbuf), "key_%06d", i);
        std::string ikey;
        AppendInternalKey(&ikey, ParsedInternalKey(kbuf, 100, kTypeValue));
        builder.Add(ikey, "val_" + std::to_string(i));
    }

    ASSERT_TRUE(builder.Finish().ok());

    auto infile = std::make_unique<MemoryRandomAccessFile>(outfile.data);
    std::unique_ptr<TableReader> reader;
    Status s = TableReader::Open(options, std::move(infile), outfile.data.size(), &reader);
    ASSERT_TRUE(s.ok()) << s.ToString();

    // Verify Bloom Filter check
    EXPECT_TRUE(reader->KeyMayMatch("key_000500"));
    EXPECT_FALSE(reader->KeyMayMatch("non_existent_key_99999"));

    // Verify TwoLevelIterator
    ReadOptions ropt;
    std::unique_ptr<Iterator> iter(reader->NewIterator(ropt));

    iter->SeekToFirst();
    int count = 0;
    while (iter->Valid()) {
        ParsedInternalKey ikey;
        ASSERT_TRUE(ParseInternalKey(iter->key(), &ikey));
        char expected_k[32];
        std::snprintf(expected_k, sizeof(expected_k), "key_%06d", count);
        EXPECT_EQ(ikey.user_key, expected_k);
        EXPECT_EQ(iter->value(), "val_" + std::to_string(count));
        count++;
        iter->Next();
    }
    EXPECT_EQ(count, 1000);
}

} // namespace lsmforge
