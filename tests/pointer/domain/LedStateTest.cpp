#include "modules/pointer/domain/LedState.hpp"

#include <gtest/gtest.h>

using ghub::pointer::LedMode;
using ghub::pointer::LedState;
using ghub::pointer::LedStateSpec;
using ghub::shared_kernel::ErrorCode;

namespace {

// LED l0 do G703 (evidence): Modes 0 1 2 3, Mode 0 (off), Color (0,0,0).
LedStateSpec g703Led()
{
    return {
        .index = 0,
        .mode = LedMode::Off,
        .rawMode = 0,
        .red = 0,
        .green = 0,
        .blue = 0,
        .colorDepth = 1,
        .brightness = 255,
        .effectDuration = 0,
        .supportedModes = {LedMode::Off, LedMode::On, LedMode::Cycle, LedMode::Breathing},
    };
}

} // namespace

TEST(LedStateTest, CreatesTheRealLed)
{
    const auto led = LedState::create(g703Led());

    ASSERT_TRUE(led.has_value()) << led.error().detail;
    EXPECT_EQ(led->index(), 0U);
    EXPECT_EQ(led->mode(), LedMode::Off);
    EXPECT_EQ(led->red(), 0U);
    EXPECT_EQ(led->supportedModes().size(), 4U);
}

TEST(LedStateTest, AcceptsColorLimits)
{
    auto spec = g703Led();
    spec.red = 255;
    spec.green = 255;
    spec.blue = 255;
    spec.brightness = 255;

    EXPECT_TRUE(LedState::create(spec).has_value());
}

TEST(LedStateTest, RejectsRedAbove255)
{
    auto spec = g703Led();
    spec.red = 256;
    EXPECT_EQ(LedState::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(LedStateTest, RejectsGreenAbove255)
{
    auto spec = g703Led();
    spec.green = 256;
    EXPECT_EQ(LedState::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(LedStateTest, RejectsBlueAbove255)
{
    auto spec = g703Led();
    spec.blue = 256;
    EXPECT_EQ(LedState::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(LedStateTest, RejectsBrightnessAbove255)
{
    auto spec = g703Led();
    spec.brightness = 256;
    EXPECT_EQ(LedState::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(LedStateTest, KeepsRawValueOfUnknownMode)
{
    auto spec = g703Led();
    spec.mode = LedMode::Unknown;
    spec.rawMode = 42;

    const auto led = LedState::create(spec);

    ASSERT_TRUE(led.has_value());
    EXPECT_EQ(led->mode(), LedMode::Unknown);
    EXPECT_EQ(led->rawMode(), 42U);
}

TEST(LedStateTest, RejectsRepeatedSupportedMode)
{
    auto spec = g703Led();
    spec.supportedModes = {LedMode::On, LedMode::On};
    EXPECT_EQ(LedState::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(LedStateTest, UnknownIsNotASupportedMode)
{
    auto spec = g703Led();
    spec.supportedModes = {LedMode::Unknown};
    EXPECT_EQ(LedState::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(LedStateTest, ModeFromCodeMapsRatbagValues)
{
    EXPECT_EQ(ghub::pointer::ledModeFromCode(0), LedMode::Off);
    EXPECT_EQ(ghub::pointer::ledModeFromCode(1), LedMode::On);
    EXPECT_EQ(ghub::pointer::ledModeFromCode(2), LedMode::Cycle);
    EXPECT_EQ(ghub::pointer::ledModeFromCode(3), LedMode::Breathing);
    EXPECT_EQ(ghub::pointer::ledModeFromCode(4), LedMode::Unknown);
}
