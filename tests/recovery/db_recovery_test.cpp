#include "lsmforge/db.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(DBRecoveryTest, PersistenceAcrossReopen) {
    std::string dbname = "./lsmforge_test_recovery";
    Options options;
    options.create_if_missing = true;
    DestroyDB(dbname, options);

    // 1. First session: insert keys into MemTable and close cleanly
    {
        DB* db = nullptr;
        ASSERT_TRUE(DB::Open(options, dbname, &db).ok());
        WriteOptions wopt;
        wopt.sync = true;

        for (int i = 0; i < 500; ++i) {
            std::string k = "persist_key_" + std::to_string(i);
            std::string v = "persist_val_" + std::to_string(i);
            ASSERT_TRUE(db->Put(wopt, k, v).ok());
        }

        // Intentionally close without manual flush to test WAL replay!
        delete db;
    }

    // 2. Second session: reopen database and verify all 500 keys are present
    {
        DB* db = nullptr;
        options.create_if_missing = false; // Must already exist!
        ASSERT_TRUE(DB::Open(options, dbname, &db).ok());
        ReadOptions ropt;

        for (int i = 0; i < 500; ++i) {
            std::string k = "persist_key_" + std::to_string(i);
            std::string expected_v = "persist_val_" + std::to_string(i);
            std::string actual_v;
            Status s = db->Get(ropt, k, &actual_v);
            ASSERT_TRUE(s.ok()) << "Missing key: " << k;
            EXPECT_EQ(actual_v, expected_v);
        }

        // Insert additional keys and perform updates/deletes
        WriteOptions wopt;
        ASSERT_TRUE(db->Put(wopt, "persist_key_10", "new_val_10").ok());
        ASSERT_TRUE(db->Delete(wopt, "persist_key_20").ok());

        delete db;
    }

    // 3. Third session: reopen again and verify modifications persisted
    {
        DB* db = nullptr;
        ASSERT_TRUE(DB::Open(options, dbname, &db).ok());
        ReadOptions ropt;

        std::string val;
        ASSERT_TRUE(db->Get(ropt, "persist_key_10", &val).ok());
        EXPECT_EQ(val, "new_val_10");

        EXPECT_TRUE(db->Get(ropt, "persist_key_20", &val).IsNotFound());

        delete db;
    }

    DestroyDB(dbname, options);
}

} // namespace lsmforge
