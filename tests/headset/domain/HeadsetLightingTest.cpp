#include "modules/headset/domain/HeadsetLighting.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using ghub::headset::HeadsetLighting;
using ghub::headset::LightingEffect;
using ghub::headset::LightingZone;
using ghub::headset::Rgb;
using ghub::shared_kernel::ErrorCode;

namespace {

using Bytes = std::vector<std::uint8_t>;

HeadsetLighting make(LightingZone zone, LightingEffect effect, Rgb color, int period, int brightness)
{
    const auto lighting = HeadsetLighting::create(zone, effect, color, period, brightness);
    EXPECT_TRUE(lighting.has_value());
    return lighting.value();
}

} // namespace

// Bytes das capturas C-01 e C-03 do G733 (0x8070 fn3).
TEST(HeadsetLightingTest, OffIsZoneAndEffectZero)
{
    EXPECT_EQ(HeadsetLighting::off(LightingZone::Lower).payload(), (Bytes{0x00, 0x00}));
    EXPECT_EQ(HeadsetLighting::off(LightingZone::Upper).payload(), (Bytes{0x01, 0x00}));
}

TEST(HeadsetLightingTest, FixedIsRgbThenTwo)
{
    const auto lighting = make(LightingZone::Upper, LightingEffect::Fixed, {0x00, 0xFF, 0xFF}, 0, 0);
    EXPECT_EQ(lighting.payload(), (Bytes{0x01, 0x01, 0x00, 0xFF, 0xFF, 0x02}));
}

TEST(HeadsetLightingTest, BreathingIsRgbPeriodZeroBrightness)
{
    const auto lighting = make(LightingZone::Upper, LightingEffect::Breathing, {0x00, 0xFF, 0xFF}, 5000, 100);
    EXPECT_EQ(lighting.payload(), (Bytes{0x01, 0x02, 0x00, 0xFF, 0xFF, 0x13, 0x88, 0x00, 0x64}));
}

TEST(HeadsetLightingTest, CycleIsFiveZerosPeriodBrightness)
{
    const auto lighting = make(LightingZone::Lower, LightingEffect::Cycle, {0x12, 0x34, 0x56}, 5000, 49);
    EXPECT_EQ(lighting.payload(), (Bytes{0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x31}))
        << "a cor é ignorada no ciclo";
    EXPECT_EQ(make(LightingZone::Lower, LightingEffect::Cycle, {0, 0, 0}, 5000, 100).payload(),
              (Bytes{0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x88, 0x64}));
}

TEST(HeadsetLightingTest, PeriodRangeIsOneToTwentyThousandInStepsOfHundred)
{
    for (const int ok : {1000, 1100, 4500, 20000}) {
        EXPECT_TRUE(HeadsetLighting::create(LightingZone::Lower, LightingEffect::Cycle, {}, ok, 50).has_value()) << ok;
    }
    for (const int bad : {0, 900, 1050, 20100, 20001, -1000}) {
        const auto lighting = HeadsetLighting::create(LightingZone::Lower, LightingEffect::Breathing, {}, bad, 50);
        ASSERT_FALSE(lighting.has_value()) << bad;
        EXPECT_EQ(lighting.error().code, ErrorCode::InvalidArgument);
    }
}

TEST(HeadsetLightingTest, BrightnessRangeIsOneToHundred)
{
    for (const int ok : {1, 50, 100}) {
        EXPECT_TRUE(HeadsetLighting::create(LightingZone::Upper, LightingEffect::Cycle, {}, 5000, ok).has_value());
    }
    for (const int bad : {0, -1, 101}) {
        EXPECT_FALSE(HeadsetLighting::create(LightingZone::Upper, LightingEffect::Breathing, {}, 5000, bad).has_value())
            << bad;
    }
}

TEST(HeadsetLightingTest, OffAndFixedIgnorePeriodAndBrightness)
{
    EXPECT_TRUE(HeadsetLighting::create(LightingZone::Lower, LightingEffect::Off, {}, 0, 0).has_value());
    EXPECT_TRUE(HeadsetLighting::create(LightingZone::Lower, LightingEffect::Fixed, {1, 2, 3}, 7, 999).has_value());
}

TEST(HeadsetLightingTest, ExposesWhatWasChosen)
{
    const auto lighting = make(LightingZone::Upper, LightingEffect::Breathing, {1, 2, 3}, 2000, 30);
    EXPECT_EQ(lighting.zone(), LightingZone::Upper);
    EXPECT_EQ(lighting.effect(), LightingEffect::Breathing);
    EXPECT_EQ(lighting, make(LightingZone::Upper, LightingEffect::Breathing, {1, 2, 3}, 2000, 30));
}
