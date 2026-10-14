#include "modules/battery/domain/BatteryLevel.hpp"

#include <gtest/gtest.h>

using ghub::battery::BatteryLevel;

TEST(BatteryLevel, Accepts_Bounds)
{
    auto empty = BatteryLevel::fromPercent(0);
    auto full = BatteryLevel::fromPercent(100);

    ASSERT_TRUE(empty.has_value());
    ASSERT_TRUE(full.has_value());
    EXPECT_EQ(empty->percent(), 0);
    EXPECT_EQ(full->percent(), 100);
}

TEST(BatteryLevel, Rejects_OutOfRange)
{
    auto below = BatteryLevel::fromPercent(-1);
    auto above = BatteryLevel::fromPercent(101);

    ASSERT_FALSE(below.has_value());
    ASSERT_FALSE(above.has_value());
    EXPECT_EQ(below.error().code, ghub::shared_kernel::ErrorCode::InvalidArgument);
    EXPECT_EQ(above.error().code, ghub::shared_kernel::ErrorCode::InvalidArgument);
}
