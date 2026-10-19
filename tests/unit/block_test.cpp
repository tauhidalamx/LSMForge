#include "table/block.h"
#include "table/block_builder.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(BlockTest, BuildAndIterate) {
    Options options;
    options.block_restart_interval = 4;
    BlockBuilder builder(&options);

    builder.Add("apple", "red");
    builder.Add("application", "code");
    builder.Add("banana", "yellow");
    builder.Add("cherry", "dark_red");

    Slice raw = builder.Finish();

    BlockContents contents;
    contents.data = raw;
    contents.heap_allocated = false;
    Block block(std::move(contents));

    std::unique_ptr<Iterator> iter(block.NewIterator(BytewiseComparator()));

    // Forward iteration
    iter->SeekToFirst();
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "apple");
    EXPECT_EQ(iter->value(), "red");

    iter->Next();
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "application");
    EXPECT_EQ(iter->value(), "code");

    iter->Next();
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "banana");
    EXPECT_EQ(iter->value(), "yellow");

    iter->Next();
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "cherry");
    EXPECT_EQ(iter->value(), "dark_red");

    iter->Next();
    EXPECT_FALSE(iter->Valid());

    // Binary search Seek
    iter->Seek("app");
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "apple");

    iter->Seek("application");
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "application");

    iter->Seek("b");
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "banana");

    iter->Seek("zebra");
    EXPECT_FALSE(iter->Valid());
}

} // namespace lsmforge
