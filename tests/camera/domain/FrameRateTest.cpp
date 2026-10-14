#include "modules/camera/domain/FrameRate.hpp"

#include <gtest/gtest.h>

using ghub::camera::FrameRate;
using ghub::shared_kernel::ErrorCode;

TEST(FrameRate, Rejects_ZeroFrames)
{
    EXPECT_EQ(FrameRate::create(0, 1).error().code, ErrorCode::InvalidArgument);
}

TEST(FrameRate, Rejects_ZeroSeconds)
{
    EXPECT_EQ(FrameRate::create(30, 0).error().code, ErrorCode::InvalidArgument);
}

TEST(FrameRate, ReducesFraction)
{
    auto rate = FrameRate::create(60, 2);

    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(rate->frames(), 30U);
    EXPECT_EQ(rate->seconds(), 1U);
    EXPECT_EQ(*rate, FrameRate::create(30, 1).value());
}

TEST(FrameRate, KeepsIrreducibleFraction)
{
    auto rate = FrameRate::create(30000, 1001);

    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(rate->frames(), 30000U);
    EXPECT_EQ(rate->seconds(), 1001U);
}

TEST(FrameRate, ComparesByValue)
{
    const auto fps7_5 = FrameRate::create(15, 2).value();
    const auto fps10 = FrameRate::create(10, 1).value();
    const auto fps30 = FrameRate::create(30, 1).value();

    EXPECT_LT(fps7_5, fps10);
    EXPECT_LT(fps10, fps30);
    EXPECT_GT(fps30, fps7_5);
    EXPECT_NE(fps7_5, fps10);
}

TEST(FrameRate, ComparesWithoutOverflow)
{
    const auto a = FrameRate::create(4294967295U, 2U).value();
    const auto b = FrameRate::create(4294967295U, 3U).value();

    EXPECT_GT(a, b);
}
