#include "lsmforge/slice.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(SliceTest, BasicOperations) {
    Slice a("hello");
    EXPECT_EQ(a.size(), 5);
    EXPECT_EQ(a.ToString(), "hello");
    EXPECT_EQ(a[0], 'h');
    EXPECT_EQ(a[4], 'o');

    Slice b("world");
    EXPECT_NE(a, b);
    EXPECT_LT(a, b);

    Slice c("hello");
    EXPECT_EQ(a, c);

    Slice sub("hello world");
    EXPECT_TRUE(sub.starts_with(a));
    sub.remove_prefix(6);
    EXPECT_EQ(sub, b);
}

TEST(SliceTest, BinarySafeWithNullBytes) {
    char data[] = {'a', '\0', 'b', '\0', 'c'};
    Slice s(data, sizeof(data));
    EXPECT_EQ(s.size(), 5);
    EXPECT_EQ(s[1], '\0');
    EXPECT_EQ(s[3], '\0');

    Slice s2(data, sizeof(data));
    EXPECT_EQ(s, s2);
    EXPECT_EQ(s.compare(s2), 0);
}

} // namespace lsmforge
