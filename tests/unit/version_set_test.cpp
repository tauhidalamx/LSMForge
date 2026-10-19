#include "db/version_set.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(VersionSetTest, EmptyVersionSet) {
    Options options;
    InternalKeyComparator icmp(BytewiseComparator());
    TableCache cache("./dummy", options, 10);
    VersionSet vset("./dummy", &options, &cache, &icmp);

    EXPECT_NE(vset.current(), nullptr);
    EXPECT_EQ(vset.NumLevelFiles(0), 0);
    EXPECT_EQ(vset.NumLevelBytes(0), 0);
    EXPECT_EQ(vset.PickCompaction(), nullptr);
}

} // namespace lsmforge
