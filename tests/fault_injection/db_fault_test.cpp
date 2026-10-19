#include "db/filename.h"
#include "lsmforge/db.h"
#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

namespace lsmforge {

TEST(DBFaultTest, TruncatedTrailingWALRecord) {
    std::string dbname = "./lsmforge_test_fault";
    Options options;
    options.create_if_missing = true;
    DestroyDB(dbname, options);

    // 1. Write several durable records
    {
        DB* db = nullptr;
        ASSERT_TRUE(DB::Open(options, dbname, &db).ok());
        WriteOptions wopt;
        wopt.sync = true;

        for (int i = 0; i < 20; ++i) {
            std::string k = "valid_key_" + std::to_string(i);
            std::string v = "valid_val_" + std::to_string(i);
            ASSERT_TRUE(db->Put(wopt, k, v).ok());
        }
        delete db;
    }

    // 2. Find the active WAL file and append a truncated/partial record to simulate crash during write
    std::vector<std::string> filenames;
    Env::Default()->GetChildren(dbname + "/wal", &filenames);
    ASSERT_FALSE(filenames.empty());

    std::string wal_path = dbname + "/wal/" + filenames[0];
    int fd = ::open(wal_path.c_str(), O_WRONLY | O_APPEND);
    ASSERT_GE(fd, 0);

    // Write incomplete 4-byte garbage header (less than 7 bytes kHeaderSize)
    const char partial_header[] = "\xaa\xbb\xcc\xdd";
    ssize_t bytes_written = ::write(fd, partial_header, 4);
    ASSERT_EQ(bytes_written, 4);
    ::close(fd);

    // 3. Reopen the database: it must recover valid records and cleanly ignore the truncated trailing write
    {
        DB* db = nullptr;
        ASSERT_TRUE(DB::Open(options, dbname, &db).ok());
        ReadOptions ropt;

        std::string val;
        for (int i = 0; i < 20; ++i) {
            std::string k = "valid_key_" + std::to_string(i);
            std::string expected_v = "valid_val_" + std::to_string(i);
            ASSERT_TRUE(db->Get(ropt, k, &val).ok());
            EXPECT_EQ(val, expected_v);
        }

        // Subsequent writes must continue to succeed
        WriteOptions wopt;
        ASSERT_TRUE(db->Put(wopt, "post_recovery_key", "success").ok());
        ASSERT_TRUE(db->Get(ropt, "post_recovery_key", &val).ok());
        EXPECT_EQ(val, "success");

        delete db;
    }

    DestroyDB(dbname, options);
}

} // namespace lsmforge
