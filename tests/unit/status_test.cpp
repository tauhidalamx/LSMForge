#include "lsmforge/status.h"
#include <gtest/gtest.h>

namespace lsmforge {

TEST(StatusTest, OKStatus) {
    Status s = Status::OK();
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(s.code(), Status::kOk);
    EXPECT_EQ(s.ToString(), "OK");
}

TEST(StatusTest, ErrorStatuses) {
    Status s1 = Status::NotFound("Key not found", "key123");
    EXPECT_FALSE(s1.ok());
    EXPECT_TRUE(s1.IsNotFound());
    EXPECT_NE(s1.ToString().find("key123"), std::string::npos);

    Status s2 = Status::Corruption("Checksum failed");
    EXPECT_TRUE(s2.IsCorruption());

    Status s3 = Status::IOError("Disk full");
    EXPECT_TRUE(s3.IsIOError());

    Status s4 = Status::InvalidArgument("Bad parameter");
    EXPECT_TRUE(s4.IsInvalidArgument());

    Status s5 = Status::Busy("Lock held");
    EXPECT_TRUE(s5.IsBusy());

    Status s6 = Status::UnsupportedOperation("Feature disabled");
    EXPECT_TRUE(s6.IsUnsupportedOperation());
}

TEST(StatusTest, CopyAndMove) {
    Status s1 = Status::NotFound("Original");
    Status s2 = s1;
    EXPECT_TRUE(s2.IsNotFound());

    Status s3 = std::move(s1);
    EXPECT_TRUE(s3.IsNotFound());
}

} // namespace lsmforge
