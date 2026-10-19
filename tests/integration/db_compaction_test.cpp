#include "lsmforge/db.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(DBCompactionTest, CompactionMaintainsCorrectness) {
    std::string dbname = "./lsmforge_test_compact";
    Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 32 * 1024; // 32KB
    DestroyDB(dbname, options);

    DB* db = nullptr;
    ASSERT_TRUE(DB::Open(options, dbname, &db).ok());

    WriteOptions wopt;
    const int n = 3000;

    // 1. Insert 3000 keys (multiple flushes to L0)
    for (int i = 0; i < n; ++i) {
        std::string k = "user_" + std::to_string(i);
        std::string v = "initial_val_" + std::to_string(i);
        ASSERT_TRUE(db->Put(wopt, k, v).ok());
    }

    // 2. Delete the first 1000 keys
    for (int i = 0; i < 1000; ++i) {
        std::string k = "user_" + std::to_string(i);
        ASSERT_TRUE(db->Delete(wopt, k).ok());
    }

    // 3. Update the second 1000 keys
    for (int i = 1000; i < 2000; ++i) {
        std::string k = "user_" + std::to_string(i);
        std::string v = "updated_val_" + std::to_string(i);
        ASSERT_TRUE(db->Put(wopt, k, v).ok());
    }

    // 4. Force full leveled compaction
    ASSERT_TRUE(db->Flush(FlushOptions()).ok());
    ASSERT_TRUE(db->Compact().ok());

    // 5. Verify correctness:
    ReadOptions ropt;
    std::string val;

    // Keys 0..999 must remain deleted and NOT resurrect old values
    for (int i = 0; i < 1000; ++i) {
        std::string k = "user_" + std::to_string(i);
        EXPECT_TRUE(db->Get(ropt, k, &val).IsNotFound()) << "Key resurrected: " << k;
    }

    // Keys 1000..1999 must have updated values
    for (int i = 1000; i < 2000; ++i) {
        std::string k = "user_" + std::to_string(i);
        ASSERT_TRUE(db->Get(ropt, k, &val).ok());
        EXPECT_EQ(val, "updated_val_" + std::to_string(i));
    }

    // Keys 2000..2999 must have initial values
    for (int i = 2000; i < n; ++i) {
        std::string k = "user_" + std::to_string(i);
        ASSERT_TRUE(db->Get(ropt, k, &val).ok());
        EXPECT_EQ(val, "initial_val_" + std::to_string(i));
    }

    delete db;
    DestroyDB(dbname, options);
}

} // namespace lsmforge
