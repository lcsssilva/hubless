#include "modules/lighting/domain/HeadsetEffect.hpp"

#include <gtest/gtest.h>

using namespace ghub::lighting::domain;
using ghub::shared_kernel::ErrorCode;

namespace {

HeadsetZone zone(std::uint32_t index)
{
    return *HeadsetZone::create(index);
}

} // namespace

TEST(HeadsetZoneTest, AcceptsZeroAndTheMaximumIndex)
{
    EXPECT_EQ(zone(0).index(), 0);
    EXPECT_EQ(zone(255).index(), 255);
}

TEST(HeadsetZoneTest, RejectsIndexAboveTheByteRange)
{
    const auto created = HeadsetZone::create(256);

    ASSERT_FALSE(created.has_value());
    EXPECT_EQ(created.error().code, ErrorCode::InvalidArgument);
}

TEST(HeadsetEffectTest, OffCarriesOnlyItsZone)
{
    const auto effect = HeadsetEffect::off(zone(1));

    EXPECT_EQ(effect.kind(), HeadsetEffectKind::Off);
    EXPECT_EQ(effect.zone(), zone(1));
    EXPECT_FALSE(effect.color().has_value());
    EXPECT_FALSE(effect.period().has_value());
    EXPECT_FALSE(effect.brightness().has_value());
}

TEST(HeadsetEffectTest, StaticCarriesZoneAndColor)
{
    const auto effect = HeadsetEffect::staticColor(zone(0), RgbColor{1, 2, 3});

    EXPECT_EQ(effect.kind(), HeadsetEffectKind::Static);
    EXPECT_EQ(effect.zone(), zone(0));
    EXPECT_EQ(effect.color(), (RgbColor{1, 2, 3}));
    EXPECT_FALSE(effect.period().has_value());
    EXPECT_FALSE(effect.brightness().has_value());
}

TEST(HeadsetEffectTest, BreathingCarriesZoneColorPeriodAndBrightness)
{
    // 4000 ms e 100 %: valores que o HeadsetControl 4.1.0 envia (logitech_g633_g933_935.hpp:91-120).
    const auto effect = HeadsetEffect::breathing(zone(0), RgbColor{9, 8, 7}, *EffectPeriod::create(4000),
                                                 *EffectBrightness::create(100));

    EXPECT_EQ(effect.kind(), HeadsetEffectKind::Breathing);
    EXPECT_EQ(effect.zone(), zone(0));
    EXPECT_EQ(effect.color(), (RgbColor{9, 8, 7}));
    EXPECT_EQ(effect.period(), *EffectPeriod::create(4000));
    EXPECT_EQ(effect.brightness(), *EffectBrightness::create(100));
}

TEST(HeadsetEffectTest, ComparesByZoneKindAndParameters)
{
    EXPECT_EQ(HeadsetEffect::off(zone(0)), HeadsetEffect::off(zone(0)));
    EXPECT_NE(HeadsetEffect::off(zone(0)), HeadsetEffect::off(zone(1)));
    EXPECT_NE(HeadsetEffect::off(zone(0)), HeadsetEffect::staticColor(zone(0), RgbColor{0, 0, 0}));
}
