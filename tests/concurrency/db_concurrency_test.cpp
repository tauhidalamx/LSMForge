#include "lsmforge/db.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace lsmforge {

TEST(DBConcurrencyTest, ConcurrentReadersAndWriters) {
    std::string dbname = "./lsmforge_test_concurrency";
    Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 128 * 1024; // 128KB
    DestroyDB(dbname, options);

    DB* db = nullptr;
    ASSERT_TRUE(DB::Open(options, dbname, &db).ok());

    const int num_writers = 4;
    const int num_readers = 4;
    const int ops_per_writer = 1000;

    std::atomic<bool> start_signal{false};
    std::atomic<bool> writers_done{false};

    // 1. Writer threads
    std::vector<std::thread> writers;
    for (int w = 0; w < num_writers; ++w) {
        writers.emplace_back([db, w, &start_signal]() {
            while (!start_signal.load()) {
                std::this_thread::yield();
            }
            WriteOptions wopt;
            for (int i = 0; i < ops_per_writer; ++i) {
                std::string k = "c_key_" + std::to_string(w) + "_" + std::to_string(i);
                std::string v = "c_val_" + std::to_string(w) + "_" + std::to_string(i);
                Status s = db->Put(wopt, k, v);
                EXPECT_TRUE(s.ok());
            }
        });
    }

    // 2. Reader threads
    std::vector<std::thread> readers;
    for (int r = 0; r < num_readers; ++r) {
        readers.emplace_back([db, &start_signal, &writers_done]() {
            while (!start_signal.load()) {
                std::this_thread::yield();
            }
            ReadOptions ropt;
            std::string val;
            while (!writers_done.load()) {
                // Read existing or non-existing keys randomly
                db->Get(ropt, "c_key_0_10", &val);
                std::this_thread::yield();
            }
        });
    }

    // Unleash all threads simultaneously
    start_signal.store(true);

    for (auto& t : writers) {
        t.join();
    }
    writers_done.store(true);

    for (auto& t : readers) {
        t.join();
    }

    // 3. Verify all written data is intact
    ReadOptions ropt;
    for (int w = 0; w < num_writers; ++w) {
        for (int i = 0; i < ops_per_writer; ++i) {
            std::string k = "c_key_" + std::to_string(w) + "_" + std::to_string(i);
            std::string expected_v = "c_val_" + std::to_string(w) + "_" + std::to_string(i);
            std::string actual_v;
            Status s = db->Get(ropt, k, &actual_v);
            ASSERT_TRUE(s.ok()) << "Missing key: " << k;
            EXPECT_EQ(actual_v, expected_v);
        }
    }

    delete db;
    DestroyDB(dbname, options);
}

} // namespace lsmforge
