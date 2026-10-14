#include "modules/audio_processing/domain/EqualizerCurve.hpp"

#include <gtest/gtest.h>

using ghub::audio_processing::EqualizerCurve;
using ghub::shared_kernel::ErrorCode;

namespace {
constexpr std::array<int, EqualizerCurve::bandCount> zeros{};
} // namespace

TEST(EqualizerCurve, FlatHasAllBandsAtZero)
{
    EXPECT_EQ(EqualizerCurve::flat().gainsDb(), zeros);
}

TEST(EqualizerCurve, AcceptsBounds)
{
    const auto curve = EqualizerCurve::create({-12, 12, 0, 0, 0, 0, 0, 0, 0, 0});
    ASSERT_TRUE(curve.has_value());
    EXPECT_EQ(curve->gainsDb()[0], -12);
    EXPECT_EQ(curve->gainsDb()[1], 12);
}

TEST(EqualizerCurve, RejectsGainBelowMinimum)
{
    const auto curve = EqualizerCurve::create({0, 0, 0, 0, 0, 0, 0, 0, 0, -13});
    ASSERT_FALSE(curve.has_value());
    EXPECT_EQ(curve.error().code, ErrorCode::InvalidArgument);
}

TEST(EqualizerCurve, RejectsGainAboveMaximum)
{
    const auto curve = EqualizerCurve::create({13, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    ASSERT_FALSE(curve.has_value());
    EXPECT_EQ(curve.error().code, ErrorCode::InvalidArgument);
}

TEST(EqualizerCurve, WithBandReturnsNewCurveAndKeepsOriginal)
{
    const auto original = EqualizerCurve::flat();
    const auto changed = original.withBand(9, 5);
    ASSERT_TRUE(changed.has_value());
    EXPECT_EQ(changed->gainsDb()[9], 5);
    EXPECT_EQ(original.gainsDb(), zeros);
}

TEST(EqualizerCurve, WithBandRejectsInexistentBand)
{
    const auto changed = EqualizerCurve::flat().withBand(10, 0);
    ASSERT_FALSE(changed.has_value());
    EXPECT_EQ(changed.error().code, ErrorCode::InvalidArgument);
}

TEST(EqualizerCurve, WithBandRejectsGainOutOfRange)
{
    EXPECT_FALSE(EqualizerCurve::flat().withBand(0, 13).has_value());
    EXPECT_FALSE(EqualizerCurve::flat().withBand(0, -13).has_value());
}

TEST(EqualizerCurve, PreampIsZeroWhenNoGainIsPositive)
{
    EXPECT_EQ(EqualizerCurve::create({-3, -12, 0, -1, 0, 0, 0, 0, 0, 0})->preampDb(), 0);
}

TEST(EqualizerCurve, PreampCompensatesTheLargestBoost)
{
    EXPECT_EQ(EqualizerCurve::create({3, -12, 7, -1, 0, 2, 0, 0, 0, 0})->preampDb(), -7);
}

TEST(EqualizerCurve, BandFrequenciesAreTheTenOctaveBands)
{
    constexpr std::array<int, 10> expected{32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    EXPECT_EQ(EqualizerCurve::bandFrequenciesHz, expected);
}
