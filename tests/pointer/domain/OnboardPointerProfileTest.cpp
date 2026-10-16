#include "modules/pointer/domain/OnboardPointerProfile.hpp"
#include "fakes/G703SectorFixtures.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace ghub::pointer {
namespace {

using Bytes = std::vector<std::uint8_t>;

Bytes sector0001()
{
    return {fixtures::kSector0001.begin(), fixtures::kSector0001.end()};
}
Bytes sector0002()
{
    return {fixtures::kSector0002.begin(), fixtures::kSector0002.end()};
}

const PointerProfile& onlyProfile(const PointerConfiguration& configuration)
{
    EXPECT_EQ(configuration.profiles().size(), 1U);
    return configuration.profiles().front();
}

TEST(OnboardPointerProfileTest, RealSector0001BecomesTheSingleActiveProfileNumberedFromOne)
{
    const auto configuration = pointerConfigurationFromOnboardSector(1, sector0001());
    ASSERT_TRUE(configuration.has_value());
    EXPECT_EQ(configuration->name(), "G703");
    const auto& profile = onlyProfile(*configuration);
    EXPECT_EQ(profile.index(), 1U);
    EXPECT_TRUE(profile.isActive());
    EXPECT_TRUE(profile.isEnabled());
    EXPECT_EQ(profile.reportRate(), 1000U);
    EXPECT_EQ(profile.supportedReportRates(), (std::vector<std::uint32_t>{125, 250, 500, 1000}));
    EXPECT_TRUE(configuration->warnings().empty());
}

TEST(OnboardPointerProfileTest, ProfileIndexIsTheSectorNumber)
{
    const auto configuration = pointerConfigurationFromOnboardSector(3, sector0001());
    ASSERT_TRUE(configuration.has_value());
    EXPECT_EQ(onlyProfile(*configuration).index(), 3U);
}

TEST(OnboardPointerProfileTest, StagesCarryTheDefaultAsActiveAndDefault)
{
    const auto configuration = pointerConfigurationFromOnboardSector(1, sector0002());
    ASSERT_TRUE(configuration.has_value());
    const auto& stages = onlyProfile(*configuration).stages();
    ASSERT_EQ(stages.size(), 4U);
    EXPECT_EQ(stages[0].dpiX(), 400U);
    EXPECT_EQ(stages[1].dpiX(), 800U);
    EXPECT_EQ(stages[3].dpiX(), 3200U);
    for (std::size_t i = 0; i < stages.size(); ++i) {
        EXPECT_EQ(stages[i].index(), i);
        EXPECT_EQ(stages[i].isDefault(), i == 1) << i;
        EXPECT_EQ(stages[i].isActive(), i == 1) << i;
        EXPECT_FALSE(stages[i].isDisabled());
        EXPECT_TRUE(stages[i].isDpiListed()) << i;
    }
}

TEST(OnboardPointerProfileTest, ButtonsAreNumberedFromOneAndDecoded)
{
    const auto configuration = pointerConfigurationFromOnboardSector(1, sector0001());
    ASSERT_TRUE(configuration.has_value());
    const auto& buttons = onlyProfile(*configuration).buttons();
    ASSERT_EQ(buttons.size(), 6U);
    EXPECT_EQ(buttons[0].index(), 1U);
    EXPECT_EQ(buttons[0].action(), ButtonAction::button(1));
    EXPECT_EQ(buttons[4].action(), ButtonAction::button(5));
    EXPECT_EQ(buttons[5].index(), 6U);
    EXPECT_EQ(buttons[5].action(), ButtonAction::unrecognized(0x90050000U));
}

TEST(OnboardPointerProfileTest, LedsAreOffInSector0001)
{
    const auto configuration = pointerConfigurationFromOnboardSector(1, sector0001());
    ASSERT_TRUE(configuration.has_value());
    const auto& leds = onlyProfile(*configuration).leds();
    ASSERT_EQ(leds.size(), 2U);
    EXPECT_EQ(leds[0].index(), 0U);
    EXPECT_EQ(leds[0].mode(), LedMode::Off);
    EXPECT_EQ(leds[1].index(), 1U);
    EXPECT_EQ(leds[1].mode(), LedMode::Off);
}

TEST(OnboardPointerProfileTest, CycleLedCarriesPeriodAndBrightness)
{
    const auto configuration = pointerConfigurationFromOnboardSector(1, sector0002());
    ASSERT_TRUE(configuration.has_value());
    const auto& led = onlyProfile(*configuration).leds().front();
    EXPECT_EQ(led.mode(), LedMode::Cycle);
    EXPECT_EQ(led.effectDuration(), 8000U);
    EXPECT_EQ(led.brightness(), 100U);
}

TEST(OnboardPointerProfileTest, FixedLedCarriesTheColor)
{
    auto raw = sector0001();
    const Bytes fixedCyan{0x01, 0x00, 0xff, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::copy(fixedCyan.begin(), fixedCyan.end(), raw.begin() + 0xD0);
    const auto configuration = pointerConfigurationFromOnboardSector(1, raw);
    ASSERT_TRUE(configuration.has_value());
    const auto& led = onlyProfile(*configuration).leds().front();
    EXPECT_EQ(led.mode(), LedMode::On);
    EXPECT_EQ(led.red(), 0U);
    EXPECT_EQ(led.green(), 255U);
    EXPECT_EQ(led.blue(), 255U);
}

TEST(OnboardPointerProfileTest, UnknownLedFormatKeepsTheRawBytes)
{
    auto raw = sector0001();
    const Bytes ripple{0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00};
    std::copy(ripple.begin(), ripple.end(), raw.begin() + 0xD0);
    const auto configuration = pointerConfigurationFromOnboardSector(1, raw);
    ASSERT_TRUE(configuration.has_value());
    const auto& led = onlyProfile(*configuration).leds().front();
    EXPECT_EQ(led.mode(), LedMode::Unknown);
    EXPECT_EQ(led.rawMode(), 0x0bU);
    EXPECT_EQ(led.rawBytes(), ripple);
}

TEST(OnboardPointerProfileTest, UnreadableSectorIsAnError)
{
    auto raw = sector0001();
    raw[0] = 3;
    const auto configuration = pointerConfigurationFromOnboardSector(1, raw);
    ASSERT_FALSE(configuration.has_value());
    EXPECT_EQ(configuration.error().code, shared_kernel::ErrorCode::MalformedData);
}

} // namespace
} // namespace ghub::pointer
