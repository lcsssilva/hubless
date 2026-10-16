#include "modules/pointer/domain/OnboardPointerProfile.hpp"

#include "modules/pointer/domain/G703ProfileSettings.hpp"
#include "modules/pointer/domain/OnboardButtons.hpp"

#include <string>
#include <utility>
#include <vector>

namespace ghub::pointer {

namespace {

constexpr std::uint32_t kMinDpi = 100;
constexpr std::uint32_t kMaxDpi = 25600;
constexpr std::uint32_t kDpiStep = 50;
constexpr std::uint32_t kColorDepth = 8;

std::vector<std::uint32_t> supportedDpis()
{
    std::vector<std::uint32_t> dpis;
    for (std::uint32_t dpi = kMinDpi; dpi <= kMaxDpi; dpi += kDpiStep) {
        dpis.push_back(dpi);
    }
    return dpis;
}

LedStateSpec ledSpec(std::uint32_t index, const LedSetting& led)
{
    LedStateSpec spec{
        .index = index,
        .mode = LedMode::Unknown,
        .rawMode = 0,
        .red = 0,
        .green = 0,
        .blue = 0,
        .colorDepth = kColorDepth,
        .brightness = 0,
        .effectDuration = 0,
        .supportedModes = {},
    };
    switch (led.kind()) {
    case LedSetting::Kind::Off:
        spec.mode = LedMode::Off;
        break;
    case LedSetting::Kind::Fixed:
        spec.mode = LedMode::On;
        spec.red = led.red();
        spec.green = led.green();
        spec.blue = led.blue();
        break;
    case LedSetting::Kind::Cycle:
        spec.mode = LedMode::Cycle;
        spec.effectDuration = led.periodMs();
        spec.brightness = led.brightness();
        break;
    case LedSetting::Kind::Raw: {
        const auto bytes = led.encode();
        spec.mode = LedMode::Unknown;
        spec.rawMode = bytes.front();
        spec.rawBytes.assign(bytes.begin(), bytes.end());
        break;
    }
    }
    return spec;
}

} // namespace

shared_kernel::Result<PointerConfiguration> pointerConfigurationFromOnboardSector(std::uint16_t sectorNumber,
                                                                                  std::span<const std::uint8_t> sector)
{
    const auto settings = G703ProfileSettings::read(sector);
    if (!settings.has_value()) {
        return std::unexpected(settings.error());
    }

    std::vector<ResolutionStage> stages;
    const auto dpis = supportedDpis();
    for (std::uint32_t i = 0; i < settings->dpiStages().size(); ++i) {
        const bool isDefault = i == settings->defaultStage();
        auto stage = ResolutionStage::create({
            .index = i,
            .dpiX = settings->dpiStages().at(i),
            .dpiY = std::nullopt,
            .active = isDefault,
            .isDefault = isDefault,
            .disabled = false,
            .supportedDpis = dpis,
        });
        if (!stage.has_value()) {
            return std::unexpected(stage.error());
        }
        stages.push_back(std::move(*stage));
    }

    std::vector<ButtonMapping> buttons;
    std::uint32_t buttonIndex = 1;
    for (const auto& action : decodeOnboardButtons(sector)) {
        auto button = ButtonMapping::create({.index = buttonIndex++, .action = action, .supportedKinds = {}});
        if (!button.has_value()) {
            return std::unexpected(button.error());
        }
        buttons.push_back(std::move(*button));
    }

    std::vector<LedState> leds;
    const LedSetting* settingsLeds[] = {&settings->primaryLed(), &settings->logoLed()};
    std::uint32_t ledIndex = 0;
    for (const LedSetting* led : settingsLeds) {
        auto state = LedState::create(ledSpec(ledIndex++, *led));
        if (!state.has_value()) {
            return std::unexpected(state.error());
        }
        leds.push_back(std::move(*state));
    }

    auto profile = PointerProfile::create({
        .index = sectorNumber,
        .name = "",
        .active = true,
        .enabled = true,
        .reportRate = settings->reportRateHz(),
        .supportedReportRates = {125, 250, 500, 1000},
        .stages = std::move(stages),
        .buttons = std::move(buttons),
        .leds = std::move(leds),
    });
    if (!profile.has_value()) {
        return std::unexpected(profile.error());
    }
    std::vector<PointerProfile> profiles;
    profiles.push_back(std::move(*profile));
    return PointerConfiguration::create("G703", std::move(profiles));
}

} // namespace ghub::pointer
