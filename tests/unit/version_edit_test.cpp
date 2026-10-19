#include "db/version_edit.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(VersionEditTest, EncodeDecode) {
    VersionEdit edit;
    edit.SetComparatorName("lsmforge.BytewiseComparator");
    edit.SetLogNumber(42);
    edit.SetNextFile(100);
    edit.SetLastSequence(500);

    edit.AddFile(0, 10, 2048, "a", "m");
    edit.AddFile(1, 11, 4096, "n", "z");
    edit.DeleteFile(0, 5);

    std::string encoded;
    edit.EncodeTo(&encoded);

    VersionEdit decoded;
    Status s = decoded.DecodeFrom(encoded);
    ASSERT_TRUE(s.ok()) << s.ToString();

    EXPECT_TRUE(decoded.has_comparator());
    EXPECT_EQ(decoded.comparator(), "lsmforge.BytewiseComparator");
    EXPECT_TRUE(decoded.has_log_number());
    EXPECT_EQ(decoded.log_number(), 42);
    EXPECT_TRUE(decoded.has_next_file_number());
    EXPECT_EQ(decoded.next_file_number(), 100);
    EXPECT_TRUE(decoded.has_last_sequence());
    EXPECT_EQ(decoded.last_sequence(), 500);

    EXPECT_EQ(decoded.deleted_files().size(), 1);
    EXPECT_TRUE(decoded.deleted_files().count(std::make_pair(0, 5)) == 1);

    EXPECT_EQ(decoded.new_files().size(), 2);
    EXPECT_EQ(decoded.new_files()[0].first, 0);
    EXPECT_EQ(decoded.new_files()[0].second.number, 10);
    EXPECT_EQ(decoded.new_files()[0].second.file_size, 2048);
}

} // namespace lsmforge
