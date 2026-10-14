#include "modules/lighting/infrastructure/G915EffectFrames.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace {

using ghub::lighting::domain::EffectBrightness;
using ghub::lighting::domain::EffectPeriod;
using ghub::lighting::domain::KeyboardEffect;
using ghub::lighting::domain::RgbColor;
using ghub::lighting::domain::RippleSpeed;
using ghub::lighting::infrastructure::encodeG915Effect;
using Params = std::array<std::uint8_t, 10>;

EffectPeriod period(std::uint32_t ms)
{
    return *EffectPeriod::create(ms); // NOLINT(bugprone-unchecked-optional-access)
}
EffectBrightness brightness(std::uint32_t percent)
{
    return *EffectBrightness::create(percent); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(G915EffectFramesTest, OffIsIdZeroWithAllParametersZero)
{
    const auto encoded = encodeG915Effect(KeyboardEffect::off());

    EXPECT_EQ(encoded.effectId, 0x0000);
    EXPECT_EQ(encoded.params, (Params{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST(G915EffectFramesTest, StaticCarriesTheColorAndTheFixedMarker)
{
    const auto encoded = encodeG915Effect(KeyboardEffect::staticColor(RgbColor{0xFF, 0x10, 0x20}));

    EXPECT_EQ(encoded.effectId, 0x0001);
    EXPECT_EQ(encoded.params, (Params{0xFF, 0x10, 0x20, 0x02, 0, 0, 0, 0, 0, 0}));
}

TEST(G915EffectFramesTest, BreathingCarriesColorBigEndianPeriodAndBrightness)
{
    const auto encoded = encodeG915Effect(KeyboardEffect::breathing(RgbColor{1, 2, 3}, period(3000), brightness(50)));

    EXPECT_EQ(encoded.effectId, 0x000A);
    EXPECT_EQ(encoded.params, (Params{1, 2, 3, 0x0B, 0xB8, 0, 50, 0, 0, 0}));
}

TEST(G915EffectFramesTest, CycleCarriesPeriodAtP5P6AndBrightnessAtP7)
{
    const auto encoded = encodeG915Effect(KeyboardEffect::cycle(period(2000), brightness(80)));

    EXPECT_EQ(encoded.effectId, 0x0003);
    EXPECT_EQ(encoded.params, (Params{0, 0, 0, 0, 0, 0x07, 0xD0, 80, 0, 0}));
}

TEST(G915EffectFramesTest, WaveSplitsThePeriodAndFixesTheHorizontalDirection)
{
    const auto encoded = encodeG915Effect(KeyboardEffect::wave(period(5000), brightness(100)));

    EXPECT_EQ(encoded.effectId, 0x0004);
    EXPECT_EQ(encoded.params, (Params{0, 0, 0, 0, 0, 0, 0x88, 0x01, 0x64, 0x13}));
}

TEST(G915EffectFramesTest, RippleCarriesColorAndTheSpeedAtP5)
{
    const auto encoded = encodeG915Effect(KeyboardEffect::ripple(RgbColor{1, 2, 3}, *RippleSpeed::create(20)));

    EXPECT_EQ(encoded.effectId, 0x000B);
    EXPECT_EQ(encoded.params, (Params{1, 2, 3, 0, 0, 20, 0, 0, 0, 0}));
}

} // namespace
