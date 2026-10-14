#include "modules/lighting/domain/EffectParameters.hpp"

#include <gtest/gtest.h>

using ghub::lighting::domain::EffectBrightness;
using ghub::lighting::domain::EffectPeriod;
using ghub::lighting::domain::RippleSpeed;
using ghub::shared_kernel::ErrorCode;

TEST(EffectPeriodTest, RejectsOneMillisecondBelowTheFastestPeriod)
{
    const auto period = EffectPeriod::create(999);

    ASSERT_FALSE(period.has_value());
    EXPECT_EQ(period.error().code, ErrorCode::InvalidArgument);
}

TEST(EffectPeriodTest, AcceptsTheFastestPeriod)
{
    const auto period = EffectPeriod::create(1000);

    ASSERT_TRUE(period.has_value());
    EXPECT_EQ(period->milliseconds(), 1000U);
}

TEST(EffectPeriodTest, AcceptsTheSlowestPeriod)
{
    const auto period = EffectPeriod::create(20000);

    ASSERT_TRUE(period.has_value());
    EXPECT_EQ(period->milliseconds(), 20000U);
}

TEST(EffectPeriodTest, RejectsOneMillisecondAboveTheSlowestPeriod)
{
    const auto period = EffectPeriod::create(20001);

    ASSERT_FALSE(period.has_value());
    EXPECT_EQ(period.error().code, ErrorCode::InvalidArgument);
}

TEST(EffectPeriodTest, ComparesByValue)
{
    EXPECT_EQ(*EffectPeriod::create(4000), *EffectPeriod::create(4000));
    EXPECT_NE(*EffectPeriod::create(4000), *EffectPeriod::create(4100));
}

TEST(EffectBrightnessTest, RejectsZeroPercent)
{
    const auto brightness = EffectBrightness::create(0);

    ASSERT_FALSE(brightness.has_value());
    EXPECT_EQ(brightness.error().code, ErrorCode::InvalidArgument);
}

TEST(EffectBrightnessTest, AcceptsOnePercent)
{
    const auto brightness = EffectBrightness::create(1);

    ASSERT_TRUE(brightness.has_value());
    EXPECT_EQ(brightness->percent(), 1U);
}

TEST(EffectBrightnessTest, AcceptsOneHundredPercent)
{
    const auto brightness = EffectBrightness::create(100);

    ASSERT_TRUE(brightness.has_value());
    EXPECT_EQ(brightness->percent(), 100U);
}

TEST(EffectBrightnessTest, RejectsOneHundredAndOnePercent)
{
    const auto brightness = EffectBrightness::create(101);

    ASSERT_FALSE(brightness.has_value());
    EXPECT_EQ(brightness.error().code, ErrorCode::InvalidArgument);
}

TEST(RippleSpeedTest, RejectsOneBelowTheMinimum)
{
    EXPECT_FALSE(RippleSpeed::create(1).has_value());
}

TEST(RippleSpeedTest, AcceptsTheMinimum)
{
    const auto speed = RippleSpeed::create(2);

    ASSERT_TRUE(speed.has_value());
    EXPECT_EQ(speed->value(), 2U);
}

TEST(RippleSpeedTest, AcceptsTheMaximum)
{
    const auto speed = RippleSpeed::create(200);

    ASSERT_TRUE(speed.has_value());
    EXPECT_EQ(speed->value(), 200U);
}

TEST(RippleSpeedTest, RejectsOneAboveTheMaximum)
{
    const auto speed = RippleSpeed::create(201);

    ASSERT_FALSE(speed.has_value());
    EXPECT_EQ(speed.error().code, ErrorCode::InvalidArgument);
}
