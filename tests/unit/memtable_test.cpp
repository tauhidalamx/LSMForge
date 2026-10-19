#include "memtable/memtable.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(MemTableTest, AddAndGet) {
    InternalKeyComparator cmp(BytewiseComparator());
    MemTable* mem = new MemTable(cmp);
    mem->Ref();

    mem->Add(100, kTypeValue, "key1", "val1_v100");
    mem->Add(101, kTypeValue, "key1", "val1_v101");
    mem->Add(102, kTypeValue, "key2", "val2");

    std::string val;
    Status s;

    // Reading at sequence 101 should observe newest version v101
    LookupKey lkey1("key1", 101);
    EXPECT_TRUE(mem->Get(lkey1, &val, &s));
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(val, "val1_v101");

    // Reading at snapshot sequence 100 should observe earlier version v100
    LookupKey lkey1_old("key1", 100);
    EXPECT_TRUE(mem->Get(lkey1_old, &val, &s));
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(val, "val1_v100");

    // Reading before sequence 100 should not find key
    LookupKey lkey1_none("key1", 99);
    EXPECT_FALSE(mem->Get(lkey1_none, &val, &s));

    mem->Unref();
}

TEST(MemTableTest, DeletionTombstone) {
    InternalKeyComparator cmp(BytewiseComparator());
    MemTable* mem = new MemTable(cmp);
    mem->Ref();

    mem->Add(50, kTypeValue, "keyA", "valA");
    mem->Add(60, kTypeDeletion, "keyA", "");

    std::string val;
    Status s;

    // Snapshot at 50 observes value
    LookupKey lkey50("keyA", 50);
    EXPECT_TRUE(mem->Get(lkey50, &val, &s));
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(val, "valA");

    // Snapshot at 60 observes deletion tombstone (returns true, s is NotFound)
    LookupKey lkey60("keyA", 60);
    EXPECT_TRUE(mem->Get(lkey60, &val, &s));
    EXPECT_TRUE(s.IsNotFound());

    mem->Unref();
}

} // namespace lsmforge
