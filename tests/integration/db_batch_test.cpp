#include "lsmforge/db.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(DBBatchTest, AtomicBatchUpdates) {
    std::string dbname = "./lsmforge_test_batch";
    Options options;
    options.create_if_missing = true;
    DestroyDB(dbname, options);

    DB* db = nullptr;
    ASSERT_TRUE(DB::Open(options, dbname, &db).ok());

    WriteBatch batch;
    batch.Put("user:1:name", "Alice");
    batch.Put("user:1:email", "alice@example.com");
    batch.Put("user:2:name", "Bob");
    batch.Delete("user:3:name");

    WriteOptions wopt;
    ASSERT_TRUE(db->Write(wopt, &batch).ok());

    ReadOptions ropt;
    std::string val;

    ASSERT_TRUE(db->Get(ropt, "user:1:name", &val).ok());
    EXPECT_EQ(val, "Alice");

    ASSERT_TRUE(db->Get(ropt, "user:1:email", &val).ok());
    EXPECT_EQ(val, "alice@example.com");

    ASSERT_TRUE(db->Get(ropt, "user:2:name", &val).ok());
    EXPECT_EQ(val, "Bob");

    EXPECT_TRUE(db->Get(ropt, "user:3:name", &val).IsNotFound());

    delete db;
    DestroyDB(dbname, options);
}

} // namespace lsmforge
