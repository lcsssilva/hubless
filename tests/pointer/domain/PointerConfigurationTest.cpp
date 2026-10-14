#include "modules/pointer/domain/PointerConfiguration.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using ghub::pointer::PointerConfiguration;
using ghub::pointer::PointerProfile;
using ghub::pointer::PointerWarning;
using ghub::pointer::PointerWarningKind;
using ghub::pointer::ResolutionStage;
using ghub::shared_kernel::ErrorCode;

namespace {

ResolutionStage stage(std::uint32_t index, std::uint32_t dpi, bool active, bool listed)
{
    return ResolutionStage::create(
               {
                   .index = index,
                   .dpiX = dpi,
                   .dpiY = std::nullopt,
                   .active = active,
                   .isDefault = active,
                   .disabled = false,
                   .supportedDpis = listed ? std::vector<std::uint32_t>{dpi} : std::vector<std::uint32_t>{100, 200},
               })
        .value();
}

PointerProfile profile(std::uint32_t index, bool active, bool enabled, std::vector<ResolutionStage> stages = {})
{
    return PointerProfile::create({
                                      .index = index,
                                      .name = "",
                                      .active = active,
                                      .enabled = enabled,
                                      .reportRate = 1000,
                                      .supportedReportRates = {1000},
                                      .stages = std::move(stages),
                                      .buttons = {},
                                      .leds = {},
                                  })
        .value();
}

bool hasWarning(const PointerConfiguration& configuration, PointerWarningKind kind)
{
    const auto warnings = configuration.warnings();
    return std::ranges::any_of(warnings, [kind](const PointerWarning& warning) { return warning.kind == kind; });
}

} // namespace

TEST(PointerConfigurationTest, CreatesWithOneProfile)
{
    const auto configuration = PointerConfiguration::create("Logitech G703 LS", {profile(0, true, true)});

    ASSERT_TRUE(configuration.has_value()) << configuration.error().detail;
    EXPECT_EQ(configuration->name(), "Logitech G703 LS");
    EXPECT_EQ(configuration->profiles().size(), 1U);
}

TEST(PointerConfigurationTest, RejectsEmptyName)
{
    EXPECT_EQ(PointerConfiguration::create("", {profile(0, true, true)}).error().code, ErrorCode::InvalidArgument);
}

TEST(PointerConfigurationTest, RejectsNoProfiles)
{
    EXPECT_EQ(PointerConfiguration::create("G703", {}).error().code, ErrorCode::InvalidArgument);
}

TEST(PointerConfigurationTest, RejectsRepeatedProfileIndex)
{
    const auto configuration = PointerConfiguration::create("G703", {profile(2, true, true), profile(2, false, true)});

    ASSERT_FALSE(configuration.has_value());
    EXPECT_EQ(configuration.error().code, ErrorCode::InvalidArgument);
}

TEST(PointerConfigurationTest, ActiveProfileIsTheOneMarkedActive)
{
    const auto configuration =
        PointerConfiguration::create("G703", {profile(0, false, true), profile(1, true, true)}).value();

    ASSERT_NE(configuration.activeProfile(), nullptr);
    EXPECT_EQ(configuration.activeProfile()->index(), 1U);
}

TEST(PointerConfigurationTest, NoActiveProfileWarnsWithoutRejecting)
{
    const auto configuration = PointerConfiguration::create("G703", {profile(0, false, true), profile(1, false, true)});

    ASSERT_TRUE(configuration.has_value());
    EXPECT_EQ(configuration->activeProfile(), nullptr);
    EXPECT_TRUE(hasWarning(*configuration, PointerWarningKind::NoActiveProfile));
}

TEST(PointerConfigurationTest, ActiveButDisabledProfileWarnsLikeTheRealG703)
{
    // Evidência: p0 habilitado e inativo, p1 ativo e desabilitado.
    const auto configuration = PointerConfiguration::create("G703", {profile(0, false, true), profile(1, true, false)});

    ASSERT_TRUE(configuration.has_value());
    EXPECT_EQ(configuration->activeProfile()->index(), 1U);
    ASSERT_TRUE(hasWarning(*configuration, PointerWarningKind::ActiveProfileDisabled));
    EXPECT_FALSE(hasWarning(*configuration, PointerWarningKind::NoActiveProfile));
    const auto warnings = configuration->warnings();
    EXPECT_EQ(warnings.front().profileIndex, std::optional<std::uint32_t>{1});
}

TEST(PointerConfigurationTest, ActiveDpiOutsideTheListWarns)
{
    const auto configuration = PointerConfiguration::create(
        "G703", {profile(0, true, true, {stage(0, 7050, true, false), stage(1, 100, false, false)})});

    ASSERT_TRUE(configuration.has_value());
    ASSERT_TRUE(hasWarning(*configuration, PointerWarningKind::ActiveDpiNotListed));
    const auto warnings = configuration->warnings();
    EXPECT_EQ(warnings.front().profileIndex, std::optional<std::uint32_t>{0});
    EXPECT_EQ(warnings.front().stageIndex, std::optional<std::uint32_t>{0});
}

TEST(PointerConfigurationTest, InactiveStageOutsideTheListDoesNotWarn)
{
    const auto configuration = PointerConfiguration::create(
        "G703", {profile(0, true, true, {stage(0, 7050, false, false), stage(1, 100, true, true)})});

    ASSERT_TRUE(configuration.has_value());
    EXPECT_TRUE(configuration->warnings().empty());
}

TEST(PointerConfigurationTest, ActiveStageWithEmptyListDoesNotWarn)
{
    const auto emptyList = ResolutionStage::create({
                                                       .index = 0,
                                                       .dpiX = 800,
                                                       .dpiY = std::nullopt,
                                                       .active = true,
                                                       .isDefault = true,
                                                       .disabled = false,
                                                       .supportedDpis = {},
                                                   })
                               .value();

    const auto configuration = PointerConfiguration::create("G703", {profile(0, true, true, {emptyList})});

    ASSERT_TRUE(configuration.has_value());
    EXPECT_TRUE(configuration->warnings().empty());
}

TEST(PointerConfigurationTest, ConsistentConfigurationHasNoWarnings)
{
    const auto configuration =
        PointerConfiguration::create("G703", {profile(0, true, true, {stage(0, 800, true, true)})});

    ASSERT_TRUE(configuration.has_value());
    EXPECT_TRUE(configuration->warnings().empty());
}
