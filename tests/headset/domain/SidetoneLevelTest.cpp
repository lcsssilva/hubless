#include "modules/headset/domain/SidetoneLevel.hpp"

#include <gtest/gtest.h>

using ghub::headset::SidetoneLevel;
using ghub::shared_kernel::ErrorCode;

TEST(SidetoneLevel, AcceptsLowerBound)
{
    const auto level = SidetoneLevel::create(0);
    ASSERT_TRUE(level.has_value());
    EXPECT_EQ(level->value(), 0);
}

TEST(SidetoneLevel, AcceptsUpperBound)
{
    const auto level = SidetoneLevel::create(100);
    ASSERT_TRUE(level.has_value());
    EXPECT_EQ(level->value(), 100);
}

TEST(SidetoneLevel, AcceptsMiddleValue)
{
    EXPECT_EQ(SidetoneLevel::create(40)->value(), 40);
}

TEST(SidetoneLevel, RejectsBelowZero)
{
    const auto level = SidetoneLevel::create(-1);
    ASSERT_FALSE(level.has_value());
    EXPECT_EQ(level.error().code, ErrorCode::InvalidArgument);
}

TEST(SidetoneLevel, RejectsAboveHundred)
{
    const auto level = SidetoneLevel::create(101);
    ASSERT_FALSE(level.has_value());
    EXPECT_EQ(level.error().code, ErrorCode::InvalidArgument);
}

TEST(SidetoneLevel, ComparesByValue)
{
    EXPECT_EQ(SidetoneLevel::create(7).value(), SidetoneLevel::create(7).value());
    EXPECT_NE(SidetoneLevel::create(7).value(), SidetoneLevel::create(8).value());
}
