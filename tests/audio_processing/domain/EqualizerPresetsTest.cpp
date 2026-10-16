#include "modules/audio_processing/domain/EqualizerPresets.hpp"

#include <gtest/gtest.h>

using ghub::audio_processing::EqualizerCurve;
using ghub::audio_processing::EqualizerPresets;
using Gains = std::array<int, EqualizerCurve::bandCount>;

TEST(EqualizerPresets, OnlyTheTwoKnownPresetsExistInOrder)
{
    const auto presets = EqualizerPresets::all();
    ASSERT_EQ(presets.size(), 2U);
    EXPECT_EQ(presets[0].name, "Plano");
    EXPECT_EQ(presets[1].name, "Aumento de grave");
}

TEST(EqualizerPresets, FlatIsZeroEverywhere)
{
    EXPECT_EQ(EqualizerPresets::all()[0].curve.gainsDb(), (Gains{}));
}

TEST(EqualizerPresets, BassBoostMatchesTheCapture)
{
    EXPECT_EQ(EqualizerPresets::all()[1].curve.gainsDb(), (Gains{4, 3, 2, 1, 1, 0, 0, 0, 0, 0}));
}

TEST(BassTreble, ZeroIsFlat)
{
    EXPECT_EQ(EqualizerPresets::fromBassTreble(0, 0), EqualizerCurve::flat());
}

TEST(BassTreble, BassOnlyUsesWeights1_1_075_05)
{
    EXPECT_EQ(EqualizerPresets::fromBassTreble(8, 0).gainsDb(), (Gains{8, 8, 6, 4, 0, 0, 0, 0, 0, 0}));
}

TEST(BassTreble, TrebleOnlyUsesWeights05_075_1_1)
{
    EXPECT_EQ(EqualizerPresets::fromBassTreble(0, 8).gainsDb(), (Gains{0, 0, 0, 0, 0, 0, 4, 6, 8, 8}));
}

TEST(BassTreble, RoundsHalfAwayFromZero)
{
    // Grave 3: 3, 3, 2,25 -> 2, 1,5 -> 2. Agudos -3: -1,5 -> -2, -2,25 -> -2, -3, -3.
    EXPECT_EQ(EqualizerPresets::fromBassTreble(3, -3).gainsDb(), (Gains{3, 3, 2, 2, 0, 0, -2, -2, -3, -3}));
}

TEST(BassTreble, ExtremesStayWithinTwelve)
{
    EXPECT_EQ(EqualizerPresets::fromBassTreble(12, -12).gainsDb(), (Gains{12, 12, 9, 6, 0, 0, -6, -9, -12, -12}));
}

TEST(BassTreble, ValuesBeyondTheRangeAreClampedToTwelve)
{
    EXPECT_EQ(EqualizerPresets::fromBassTreble(40, -40).gainsDb(), (Gains{12, 12, 12, 12, 0, 0, -12, -12, -12, -12}));
}
