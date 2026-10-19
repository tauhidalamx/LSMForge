#include "lsmforge/db.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(DBIteratorTest, SortedScanAndSeek) {
    std::string dbname = "./lsmforge_test_iter";
    Options options;
    options.create_if_missing = true;
    DestroyDB(dbname, options);

    DB* db = nullptr;
    ASSERT_TRUE(DB::Open(options, dbname, &db).ok());

    WriteOptions wopt;
    // Insert keys in random order
    std::vector<std::string> keys = {"dog", "apple", "cat", "banana", "elephant"};
    for (const auto& k : keys) {
        ASSERT_TRUE(db->Put(wopt, k, "val_" + k).ok());
    }

    ReadOptions ropt;
    std::unique_ptr<Iterator> iter(db->NewIterator(ropt));

    // Forward iteration should be strictly sorted
    iter->SeekToFirst();
    std::vector<std::string> expected = {"apple", "banana", "cat", "dog", "elephant"};
    int idx = 0;
    while (iter->Valid()) {
        EXPECT_EQ(iter->key(), expected[idx]);
        EXPECT_EQ(iter->value(), "val_" + expected[idx]);
        idx++;
        iter->Next();
    }
    EXPECT_EQ(idx, 5);

    // Seek test
    iter->Seek("c");
    ASSERT_TRUE(iter->Valid());
    EXPECT_EQ(iter->key(), "cat");

    iter.reset();
    delete db;
    DestroyDB(dbname, options);
}

TEST(DBIteratorTest, SnapshotVisibilityAndTombstones) {
    std::string dbname = "./lsmforge_test_snapshot";
    Options options;
    options.create_if_missing = true;
    DestroyDB(dbname, options);

    DB* db = nullptr;
    ASSERT_TRUE(DB::Open(options, dbname, &db).ok());

    WriteOptions wopt;
    ASSERT_TRUE(db->Put(wopt, "key1", "v1").ok());
    ASSERT_TRUE(db->Put(wopt, "key2", "v2").ok());

    // Take snapshot
    const Snapshot* snap = db->GetSnapshot();

    // Perform updates after snapshot
    ASSERT_TRUE(db->Put(wopt, "key1", "v1_updated").ok());
    ASSERT_TRUE(db->Delete(wopt, "key2").ok());
    ASSERT_TRUE(db->Put(wopt, "key3", "v3").ok());

    // 1. Read using Snapshot
    ReadOptions snap_opt;
    snap_opt.snapshot = snap;

    std::string val;
    ASSERT_TRUE(db->Get(snap_opt, "key1", &val).ok());
    EXPECT_EQ(val, "v1"); // Sees pre-update value!

    ASSERT_TRUE(db->Get(snap_opt, "key2", &val).ok());
    EXPECT_EQ(val, "v2"); // Sees pre-deletion value!

    EXPECT_TRUE(db->Get(snap_opt, "key3", &val).IsNotFound()); // key3 was created after snapshot!

    // 2. Read current state (no snapshot)
    ReadOptions curr_opt;
    ASSERT_TRUE(db->Get(curr_opt, "key1", &val).ok());
    EXPECT_EQ(val, "v1_updated");

    EXPECT_TRUE(db->Get(curr_opt, "key2", &val).IsNotFound());

    ASSERT_TRUE(db->Get(curr_opt, "key3", &val).ok());
    EXPECT_EQ(val, "v3");

    db->ReleaseSnapshot(snap);
    delete db;
    DestroyDB(dbname, options);
}

} // namespace lsmforge
