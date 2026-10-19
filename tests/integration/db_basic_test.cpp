#include "lsmforge/db.h"
#include <gtest/gtest.h>

namespace lsmforge {

class DBBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        dbname_ = "./lsmforge_test_basic";
        options_.create_if_missing = true;
        options_.error_if_exists = false;
        options_.write_buffer_size = 64 * 1024; // 64KB to trigger easy flushes
        DestroyDB(dbname_, options_);
        Status s = DB::Open(options_, dbname_, &db_);
        ASSERT_TRUE(s.ok()) << s.ToString();
    }

    void TearDown() override {
        delete db_;
        DestroyDB(dbname_, options_);
    }

    std::string dbname_;
    Options options_;
    DB* db_{nullptr};
};

TEST_F(DBBasicTest, PutGetDelete) {
    WriteOptions wopt;
    ReadOptions ropt;
    std::string value;

    // 1. Put and Get
    ASSERT_TRUE(db_->Put(wopt, "name", "LSMForge").ok());
    ASSERT_TRUE(db_->Get(ropt, "name", &value).ok());
    EXPECT_EQ(value, "LSMForge");

    // 2. Update existing key
    ASSERT_TRUE(db_->Put(wopt, "name", "LSMForge-2.0").ok());
    ASSERT_TRUE(db_->Get(ropt, "name", &value).ok());
    EXPECT_EQ(value, "LSMForge-2.0");

    // 3. Delete key
    ASSERT_TRUE(db_->Delete(wopt, "name").ok());
    Status s = db_->Get(ropt, "name", &value);
    EXPECT_TRUE(s.IsNotFound());
}

TEST_F(DBBasicTest, EmptyKeyAndValue) {
    WriteOptions wopt;
    ReadOptions ropt;
    std::string value;

    // Empty key with non-empty value
    ASSERT_TRUE(db_->Put(wopt, "", "empty_key_val").ok());
    ASSERT_TRUE(db_->Get(ropt, "", &value).ok());
    EXPECT_EQ(value, "empty_key_val");

    // Non-empty key with empty value
    ASSERT_TRUE(db_->Put(wopt, "key_with_empty_val", "").ok());
    ASSERT_TRUE(db_->Get(ropt, "key_with_empty_val", &value).ok());
    EXPECT_EQ(value, "");

    // Both key and value empty
    ASSERT_TRUE(db_->Put(wopt, "", "").ok());
    ASSERT_TRUE(db_->Get(ropt, "", &value).ok());
    EXPECT_EQ(value, "");
}

TEST_F(DBBasicTest, BinarySafeKeysAndValues) {
    WriteOptions wopt;
    ReadOptions ropt;
    std::string value;

    // Key with embedded nulls
    std::string binary_key("prefix\0mid\0suffix", 16);
    std::string binary_val("binary\0data\0payload", 19);

    ASSERT_TRUE(db_->Put(wopt, binary_key, binary_val).ok());
    ASSERT_TRUE(db_->Get(ropt, binary_key, &value).ok());
    EXPECT_EQ(value.size(), binary_val.size());
    EXPECT_EQ(value, binary_val);

    // Delete binary key
    ASSERT_TRUE(db_->Delete(wopt, binary_key).ok());
    EXPECT_TRUE(db_->Get(ropt, binary_key, &value).IsNotFound());
}

TEST_F(DBBasicTest, FlushMultipleMemTables) {
    WriteOptions wopt;
    ReadOptions ropt;

    // Insert enough keys to exceed write_buffer_size (64KB) multiple times
    const int num_keys = 2000;
    for (int i = 0; i < num_keys; ++i) {
        std::string k = "key_" + std::to_string(i);
        std::string v = "val_" + std::string(100, 'x') + "_" + std::to_string(i);
        ASSERT_TRUE(db_->Put(wopt, k, v).ok());
    }

    // Force flush
    ASSERT_TRUE(db_->Flush(FlushOptions()).ok());

    // Verify all keys can still be read from SSTables
    for (int i = 0; i < num_keys; ++i) {
        std::string k = "key_" + std::to_string(i);
        std::string expected_v = "val_" + std::string(100, 'x') + "_" + std::to_string(i);
        std::string actual_v;
        Status s = db_->Get(ropt, k, &actual_v);
        ASSERT_TRUE(s.ok()) << "Failed on key: " << k << ", error: " << s.ToString();
        EXPECT_EQ(actual_v, expected_v);
    }
}

} // namespace lsmforge
