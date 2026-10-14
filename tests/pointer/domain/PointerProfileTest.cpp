#include "modules/pointer/domain/PointerProfile.hpp"

#include <gtest/gtest.h>

using ghub::pointer::ButtonAction;
using ghub::pointer::ButtonMapping;
using ghub::pointer::LedMode;
using ghub::pointer::LedState;
using ghub::pointer::PointerProfile;
using ghub::pointer::PointerProfileSpec;
using ghub::pointer::ResolutionStage;
using ghub::shared_kernel::ErrorCode;

namespace {

ResolutionStage stage(std::uint32_t index, std::uint32_t dpi = 100)
{
    return ResolutionStage::create({
                                       .index = index,
                                       .dpiX = dpi,
                                       .dpiY = std::nullopt,
                                       .active = false,
                                       .isDefault = false,
                                       .disabled = false,
                                       .supportedDpis = {100, 200},
                                   })
        .value();
}

ButtonMapping button(std::uint32_t index)
{
    return ButtonMapping::create({.index = index, .action = ButtonAction::button(index + 1), .supportedKinds = {}})
        .value();
}

LedState led(std::uint32_t index)
{
    return LedState::create({
                                .index = index,
                                .mode = LedMode::Off,
                                .rawMode = 0,
                                .red = 0,
                                .green = 0,
                                .blue = 0,
                                .colorDepth = 1,
                                .brightness = 255,
                                .effectDuration = 0,
                                .supportedModes = {},
                            })
        .value();
}

// Perfil p0 do G703: taxa 1000, 125/250/500/1000, 5 estágios, 6 botões, 2 LEDs.
PointerProfileSpec g703Profile0()
{
    return {
        .index = 0,
        .name = "",
        .active = false,
        .enabled = true,
        .reportRate = 1000,
        .supportedReportRates = {125, 250, 500, 1000},
        .stages = {stage(0), stage(1), stage(2), stage(3), stage(4)},
        .buttons = {button(0), button(1), button(2), button(3), button(4), button(5)},
        .leds = {led(0), led(1)},
    };
}

} // namespace

TEST(PointerProfileTest, CreatesTheRealProfile)
{
    const auto profile = PointerProfile::create(g703Profile0());

    ASSERT_TRUE(profile.has_value()) << profile.error().detail;
    EXPECT_EQ(profile->index(), 0U);
    EXPECT_FALSE(profile->isActive());
    EXPECT_TRUE(profile->isEnabled());
    EXPECT_EQ(profile->reportRate(), 1000U);
    EXPECT_EQ(profile->supportedReportRates().size(), 4U);
    EXPECT_EQ(profile->stages().size(), 5U);
    EXPECT_EQ(profile->buttons().size(), 6U);
    EXPECT_EQ(profile->leds().size(), 2U);
}

TEST(PointerProfileTest, KeepsActiveAndDisabledTogether)
{
    auto spec = g703Profile0();
    spec.index = 1;
    spec.active = true;
    spec.enabled = false;
    spec.reportRate = 0;
    spec.supportedReportRates = {};
    spec.stages = {};
    spec.buttons = {};
    spec.leds = {};

    const auto profile = PointerProfile::create(spec);

    ASSERT_TRUE(profile.has_value());
    EXPECT_TRUE(profile->isActive());
    EXPECT_FALSE(profile->isEnabled());
    EXPECT_EQ(profile->reportRate(), 0U);
}

TEST(PointerProfileTest, RejectsZeroSupportedReportRate)
{
    auto spec = g703Profile0();
    spec.supportedReportRates = {0, 125};
    EXPECT_EQ(PointerProfile::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(PointerProfileTest, RejectsRepeatedSupportedReportRate)
{
    auto spec = g703Profile0();
    spec.supportedReportRates = {125, 125};
    EXPECT_EQ(PointerProfile::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(PointerProfileTest, RejectsRepeatedStageIndex)
{
    auto spec = g703Profile0();
    spec.stages = {stage(0), stage(0)};
    EXPECT_EQ(PointerProfile::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(PointerProfileTest, RejectsRepeatedButtonIndex)
{
    auto spec = g703Profile0();
    spec.buttons = {button(1), button(1)};
    EXPECT_EQ(PointerProfile::create(spec).error().code, ErrorCode::InvalidArgument);
}

TEST(PointerProfileTest, RejectsRepeatedLedIndex)
{
    auto spec = g703Profile0();
    spec.leds = {led(1), led(1)};
    EXPECT_EQ(PointerProfile::create(spec).error().code, ErrorCode::InvalidArgument);
}
