#include "memtable/skiplist.h"
#include "util/arena.h"
#include <gtest/gtest.h>
#include <set>

namespace lsmforge {

struct IntComparator {
    int operator()(const uint64_t a, const uint64_t b) const {
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
};

TEST(SkipListTest, EmptyList) {
    Arena arena;
    IntComparator cmp;
    SkipList<uint64_t, IntComparator> list(cmp, &arena);

    EXPECT_FALSE(list.Contains(10));
    SkipList<uint64_t, IntComparator>::Iterator iter(&list);
    EXPECT_FALSE(iter.Valid());

    iter.SeekToFirst();
    EXPECT_FALSE(iter.Valid());

    iter.Seek(100);
    EXPECT_FALSE(iter.Valid());
}

TEST(SkipListTest, InsertAndLookup) {
    Arena arena;
    IntComparator cmp;
    SkipList<uint64_t, IntComparator> list(cmp, &arena);

    std::set<uint64_t> keys = {10, 50, 20, 80, 5, 99, 1000, 3};
    for (uint64_t k : keys) {
        list.Insert(k);
    }

    for (uint64_t k : keys) {
        EXPECT_TRUE(list.Contains(k));
    }
    EXPECT_FALSE(list.Contains(1));
    EXPECT_FALSE(list.Contains(4));
    EXPECT_FALSE(list.Contains(999));

    // Test ordered iteration
    SkipList<uint64_t, IntComparator>::Iterator iter(&list);
    iter.SeekToFirst();
    auto set_it = keys.begin();
    while (iter.Valid()) {
        EXPECT_EQ(iter.key(), *set_it);
        ++set_it;
        iter.Next();
    }
    EXPECT_EQ(set_it, keys.end());
}

} // namespace lsmforge
