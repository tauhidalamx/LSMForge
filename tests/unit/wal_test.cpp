#include "wal/wal_reader.h"
#include "wal/wal_writer.h"
#include <gtest/gtest.h>

namespace lsmforge {

class StringWritableFile : public WritableFile {
public:
    std::string contents;
    Status Append(const Slice& data) override {
        contents.append(data.data(), data.size());
        return Status::OK();
    }
    Status Close() override { return Status::OK(); }
    Status Flush() override { return Status::OK(); }
    Status Sync() override { return Status::OK(); }
};

class StringSequentialFile : public SequentialFile {
public:
    explicit StringSequentialFile(std::string s) : contents_(std::move(s)), offset_(0) {}
    Status Read(size_t n, Slice* result, char* scratch) override {
        size_t available = (offset_ < contents_.size()) ? (contents_.size() - offset_) : 0;
        size_t to_read = (n < available) ? n : available;
        std::memcpy(scratch, contents_.data() + offset_, to_read);
        *result = Slice(scratch, to_read);
        offset_ += to_read;
        return Status::OK();
    }
    Status Skip(uint64_t n) override {
        offset_ += n;
        return Status::OK();
    }
private:
    std::string contents_;
    size_t offset_;
};

TEST(WalTest, ShortRecords) {
    StringWritableFile outfile;
    wal::WalWriter writer(&outfile);

    writer.AddRecord("hello");
    writer.AddRecord("world");
    writer.AddRecord("antigravity");

    StringSequentialFile infile(outfile.contents);
    wal::WalReader reader(&infile, nullptr, true, 0);

    Slice record;
    std::string scratch;

    EXPECT_TRUE(reader.ReadRecord(&record, &scratch));
    EXPECT_EQ(record, "hello");

    EXPECT_TRUE(reader.ReadRecord(&record, &scratch));
    EXPECT_EQ(record, "world");

    EXPECT_TRUE(reader.ReadRecord(&record, &scratch));
    EXPECT_EQ(record, "antigravity");

    EXPECT_FALSE(reader.ReadRecord(&record, &scratch));
}

TEST(WalTest, LargeFragmentedRecord) {
    StringWritableFile outfile;
    wal::WalWriter writer(&outfile);

    // 100KB record spanning multiple 32KB blocks
    std::string large_record(100000, 'L');
    writer.AddRecord(large_record);
    writer.AddRecord("trailing_record");

    StringSequentialFile infile(outfile.contents);
    wal::WalReader reader(&infile, nullptr, true, 0);

    Slice record;
    std::string scratch;

    EXPECT_TRUE(reader.ReadRecord(&record, &scratch));
    EXPECT_EQ(record.size(), 100000);
    EXPECT_EQ(record, large_record);

    EXPECT_TRUE(reader.ReadRecord(&record, &scratch));
    EXPECT_EQ(record, "trailing_record");

    EXPECT_FALSE(reader.ReadRecord(&record, &scratch));
}

} // namespace lsmforge
