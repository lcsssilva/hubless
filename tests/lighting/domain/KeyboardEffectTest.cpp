#include "modules/lighting/domain/KeyboardEffect.hpp"

#include <gtest/gtest.h>

using namespace ghub::lighting::domain;

namespace {

EffectPeriod period(std::uint32_t ms)
{
    return *EffectPeriod::create(ms);
}
EffectBrightness brightness(std::uint32_t percent)
{
    return *EffectBrightness::create(percent);
}

} // namespace

TEST(RgbColorTest, ExposesItsChannelsAndComparesByValue)
{
    const RgbColor color{10, 20, 30};

    EXPECT_EQ(color.red(), 10);
    EXPECT_EQ(color.green(), 20);
    EXPECT_EQ(color.blue(), 30);
    EXPECT_EQ(color, (RgbColor{10, 20, 30}));
    EXPECT_NE(color, (RgbColor{10, 20, 31}));
}

TEST(KeyboardEffectTest, OffCarriesNoParameters)
{
    const KeyboardEffect effect = KeyboardEffect::off();

    EXPECT_EQ(effect.kind(), KeyboardEffectKind::Off);
    EXPECT_FALSE(effect.color().has_value());
    EXPECT_FALSE(effect.period().has_value());
    EXPECT_FALSE(effect.brightness().has_value());
    EXPECT_FALSE(effect.rippleSpeed().has_value());
}

TEST(KeyboardEffectTest, StaticCarriesOnlyItsColor)
{
    const auto effect = KeyboardEffect::staticColor(RgbColor{255, 0, 0});

    EXPECT_EQ(effect.kind(), KeyboardEffectKind::Static);
    EXPECT_EQ(effect.color(), (RgbColor{255, 0, 0}));
    EXPECT_FALSE(effect.period().has_value());
    EXPECT_FALSE(effect.brightness().has_value());
    EXPECT_FALSE(effect.rippleSpeed().has_value());
}

TEST(KeyboardEffectTest, BreathingCarriesColorPeriodAndBrightness)
{
    const auto effect = KeyboardEffect::breathing(RgbColor{0, 255, 0}, period(4000), brightness(80));

    EXPECT_EQ(effect.kind(), KeyboardEffectKind::Breathing);
    EXPECT_EQ(effect.color(), (RgbColor{0, 255, 0}));
    EXPECT_EQ(effect.period(), period(4000));
    EXPECT_EQ(effect.brightness(), brightness(80));
    EXPECT_FALSE(effect.rippleSpeed().has_value());
}

TEST(KeyboardEffectTest, CycleCarriesPeriodAndBrightnessButNoColor)
{
    const auto effect = KeyboardEffect::cycle(period(8000), brightness(50));

    EXPECT_EQ(effect.kind(), KeyboardEffectKind::Cycle);
    EXPECT_FALSE(effect.color().has_value());
    EXPECT_EQ(effect.period(), period(8000));
    EXPECT_EQ(effect.brightness(), brightness(50));
}

TEST(KeyboardEffectTest, WaveCarriesPeriodAndBrightnessButNoColor)
{
    const auto effect = KeyboardEffect::wave(period(2000), brightness(100));

    EXPECT_EQ(effect.kind(), KeyboardEffectKind::Wave);
    EXPECT_FALSE(effect.color().has_value());
    EXPECT_EQ(effect.period(), period(2000));
    EXPECT_EQ(effect.brightness(), brightness(100));
}

TEST(KeyboardEffectTest, RippleCarriesColorAndRippleSpeedButNoPeriod)
{
    const auto effect = KeyboardEffect::ripple(RgbColor{0, 0, 255}, *RippleSpeed::create(20));

    EXPECT_EQ(effect.kind(), KeyboardEffectKind::Ripple);
    EXPECT_EQ(effect.color(), (RgbColor{0, 0, 255}));
    EXPECT_EQ(effect.rippleSpeed(), *RippleSpeed::create(20));
    EXPECT_FALSE(effect.period().has_value());
    EXPECT_FALSE(effect.brightness().has_value());
}

TEST(KeyboardEffectTest, ComparesByKindAndParameters)
{
    EXPECT_EQ(KeyboardEffect::off(), KeyboardEffect::off());
    EXPECT_EQ(KeyboardEffect::cycle(period(8000), brightness(50)), KeyboardEffect::cycle(period(8000), brightness(50)));
    EXPECT_NE(KeyboardEffect::cycle(period(8000), brightness(50)), KeyboardEffect::cycle(period(8000), brightness(51)));
    EXPECT_NE(KeyboardEffect::cycle(period(8000), brightness(50)), KeyboardEffect::wave(period(8000), brightness(50)));
    EXPECT_NE(KeyboardEffect::staticColor(RgbColor{1, 2, 3}), KeyboardEffect::staticColor(RgbColor{1, 2, 4}));
}

TEST(LogoCompanionTest, BreathingStaysBreathingWithSameParameters)
{
    const auto keys = KeyboardEffect::breathing(RgbColor{1, 2, 3}, period(4000), brightness(70));
    EXPECT_EQ(logoCompanion(keys), keys);
}

TEST(LogoCompanionTest, CycleStaysCycleWithSameParameters)
{
    const auto keys = KeyboardEffect::cycle(period(8000), brightness(50));
    EXPECT_EQ(logoCompanion(keys), keys);
}

TEST(LogoCompanionTest, StaticStaysStaticWithSameColor)
{
    const auto keys = KeyboardEffect::staticColor(RgbColor{9, 8, 7});
    EXPECT_EQ(logoCompanion(keys), keys);
}

TEST(LogoCompanionTest, OffStaysOff)
{
    EXPECT_EQ(logoCompanion(KeyboardEffect::off()), KeyboardEffect::off());
}

TEST(LogoCompanionTest, WaveBecomesCycleWithSamePeriodAndBrightness)
{
    EXPECT_EQ(logoCompanion(KeyboardEffect::wave(period(5000), brightness(80))),
              KeyboardEffect::cycle(period(5000), brightness(80)));
}

TEST(LogoCompanionTest, RippleBecomesStaticWithSameColor)
{
    EXPECT_EQ(logoCompanion(KeyboardEffect::ripple(RgbColor{0, 0, 255}, *RippleSpeed::create(20))),
              KeyboardEffect::staticColor(RgbColor{0, 0, 255}));
}
